/*-------------------------------------------------------------------------
 *
 * operatorcmds.c
 *
 *	  Routines for operator manipulation commands
 *
 * Portions Copyright (c) 1996-2012, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/operatorcmds.c
 *
 * DESCRIPTION
 *	  The "DefineFoo" routines take the parse tree and pick out the
 *	  appropriate arguments/flags, passing the results to the
 *	  corresponding "FooDefine" routines (in src/catalog) that do
 *	  the actual catalog-munging.  These routines also verify permission
 *	  of the user to execute the command.
 *
 * NOTES
 *	  These things must be defined and committed in the following order:
 *		"create function":
 *				input/output, recv/send procedures
 *		"create type":
 *				type
 *		"create operator":
 *				operators
 *
 *		Most of the parse-tree manipulation routines are defined in
 *		commands/manip.c.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "commands/alter.h"
#include "commands/defrem.h"
#include "miscadmin.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "parser/parse_type.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"


static void AlterOperatorOwner_internal(Relation rel, Oid operOid, Oid newOwnerId,
										CommandContext cmd);

/*
 * DefineOperator
 *		this function extracts all the information from the
 *		parameter list generated by the parser and then has
 *		OperatorCreate() do all the actual work.
 *
 * 'parameters' is a list of DefElem
 */
void
DefineOperator(List *names, List *parameters, CommandContext cmd)
{
	char	   *oprName;
	Oid			oprNamespace;
	AclResult	aclresult;
	bool		canMerge = false;		/* operator merges */
	bool		canHash = false;	/* operator hashes */
	List	   *functionName = NIL;		/* function for operator */
	TypeName   *typeName1 = NULL;		/* first type name */
	TypeName   *typeName2 = NULL;		/* second type name */
	Oid			typeId1 = InvalidOid;	/* types converted to OID */
	Oid			typeId2 = InvalidOid;
	Oid			rettype;
	List	   *commutatorName = NIL;	/* optional commutator operator name */
	List	   *negatorName = NIL;		/* optional negator operator name */
	List	   *restrictionName = NIL;	/* optional restrict. sel. procedure */
	List	   *joinName = NIL; /* optional join sel. procedure */
	Oid			functionOid;	/* functions converted to OID */
	Oid			restrictionOid;
	Oid			joinOid;
	Oid			typeId[5];		/* only need up to 5 args here */
	int			nargs;
	ListCell   *pl;

	/* Convert list of names to a name and namespace */
	oprNamespace = QualifiedNameGetCreationNamespace(names, &oprName);

	/*
	 * The SQL standard committee has decided that => should be used for named
	 * parameters; therefore, a future release of PostgreSQL may disallow it
	 * as the name of a user-defined operator.
	 */
	if (strcmp(oprName, "=>") == 0)
		ereport(WARNING,
				(errmsg("=> is deprecated as an operator name"),
				 errdetail("This name may be disallowed altogether in future versions of PostgreSQL.")));

	/* Check we have creation rights in target namespace */
	aclresult = pg_namespace_aclcheck(oprNamespace, GetUserId(), ACL_CREATE);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, ACL_KIND_NAMESPACE,
					   get_namespace_name(oprNamespace));

	/*
	 * loop over the definition list and extract the information we need.
	 */
	foreach(pl, parameters)
	{
		DefElem    *defel = (DefElem *) lfirst(pl);

		if (pg_strcasecmp(defel->defname, "leftarg") == 0)
		{
			typeName1 = defGetTypeName(defel);
			if (typeName1->setof)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
					errmsg("SETOF type not allowed for operator argument")));
		}
		else if (pg_strcasecmp(defel->defname, "rightarg") == 0)
		{
			typeName2 = defGetTypeName(defel);
			if (typeName2->setof)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
					errmsg("SETOF type not allowed for operator argument")));
		}
		else if (pg_strcasecmp(defel->defname, "procedure") == 0)
			functionName = defGetQualifiedName(defel);
		else if (pg_strcasecmp(defel->defname, "commutator") == 0)
			commutatorName = defGetQualifiedName(defel);
		else if (pg_strcasecmp(defel->defname, "negator") == 0)
			negatorName = defGetQualifiedName(defel);
		else if (pg_strcasecmp(defel->defname, "restrict") == 0)
			restrictionName = defGetQualifiedName(defel);
		else if (pg_strcasecmp(defel->defname, "join") == 0)
			joinName = defGetQualifiedName(defel);
		else if (pg_strcasecmp(defel->defname, "hashes") == 0)
			canHash = defGetBoolean(defel);
		else if (pg_strcasecmp(defel->defname, "merges") == 0)
			canMerge = defGetBoolean(defel);
		/* These obsolete options are taken as meaning canMerge */
		else if (pg_strcasecmp(defel->defname, "sort1") == 0)
			canMerge = true;
		else if (pg_strcasecmp(defel->defname, "sort2") == 0)
			canMerge = true;
		else if (pg_strcasecmp(defel->defname, "ltcmp") == 0)
			canMerge = true;
		else if (pg_strcasecmp(defel->defname, "gtcmp") == 0)
			canMerge = true;
		else
			ereport(WARNING,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("operator attribute \"%s\" not recognized",
							defel->defname)));
	}

	/*
	 * make sure we have our required definitions
	 */
	if (functionName == NIL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
				 errmsg("operator procedure must be specified")));

	/* Transform type names to type OIDs */
	if (typeName1)
		typeId1 = typenameTypeId(NULL, typeName1);
	if (typeName2)
		typeId2 = typenameTypeId(NULL, typeName2);

	if (!OidIsValid(typeId1) && !OidIsValid(typeId2))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_FUNCTION_DEFINITION),
		   errmsg("at least one of leftarg or rightarg must be specified")));

	if (typeName1)
	{
		aclresult = pg_type_aclcheck(typeId1, GetUserId(), ACL_USAGE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_TYPE,
						   format_type_be(typeId1));
	}

	if (typeName2)
	{
		aclresult = pg_type_aclcheck(typeId2, GetUserId(), ACL_USAGE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_TYPE,
						   format_type_be(typeId2));
	}

	/*
	 * Look up the operator's underlying function.
	 */
	if (!OidIsValid(typeId1))
	{
		typeId[0] = typeId2;
		nargs = 1;
	}
	else if (!OidIsValid(typeId2))
	{
		typeId[0] = typeId1;
		nargs = 1;
	}
	else
	{
		typeId[0] = typeId1;
		typeId[1] = typeId2;
		nargs = 2;
	}
	functionOid = LookupFuncName(functionName, nargs, typeId, false);

	/*
	 * We require EXECUTE rights for the function.	This isn't strictly
	 * necessary, since EXECUTE will be checked at any attempted use of the
	 * operator, but it seems like a good idea anyway.
	 */
	aclresult = pg_proc_aclcheck(functionOid, GetUserId(), ACL_EXECUTE);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, ACL_KIND_PROC,
					   NameListToString(functionName));

	rettype = get_func_rettype(functionOid);
	aclresult = pg_type_aclcheck(rettype, GetUserId(), ACL_USAGE);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, ACL_KIND_TYPE,
					   format_type_be(rettype));

	/*
	 * Look up restriction estimator if specified
	 */
	if (restrictionName)
	{
		typeId[0] = INTERNALOID;	/* PlannerInfo */
		typeId[1] = OIDOID;		/* operator OID */
		typeId[2] = INTERNALOID;	/* args list */
		typeId[3] = INT4OID;	/* varRelid */

		restrictionOid = LookupFuncName(restrictionName, 4, typeId, false);

		/* estimators must return float8 */
		if (get_func_rettype(restrictionOid) != FLOAT8OID)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
					 errmsg("restriction estimator function %s must return type \"float8\"",
							NameListToString(restrictionName))));

		/* Require EXECUTE rights for the estimator */
		aclresult = pg_proc_aclcheck(restrictionOid, GetUserId(), ACL_EXECUTE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_PROC,
						   NameListToString(restrictionName));
	}
	else
		restrictionOid = InvalidOid;

	/*
	 * Look up join estimator if specified
	 */
	if (joinName)
	{
		typeId[0] = INTERNALOID;	/* PlannerInfo */
		typeId[1] = OIDOID;		/* operator OID */
		typeId[2] = INTERNALOID;	/* args list */
		typeId[3] = INT2OID;	/* jointype */
		typeId[4] = INTERNALOID;	/* SpecialJoinInfo */

		/*
		 * As of Postgres 8.4, the preferred signature for join estimators has
		 * 5 arguments, but we still allow the old 4-argument form. Try the
		 * preferred form first.
		 */
		joinOid = LookupFuncName(joinName, 5, typeId, true);
		if (!OidIsValid(joinOid))
			joinOid = LookupFuncName(joinName, 4, typeId, true);
		/* If not found, reference the 5-argument signature in error msg */
		if (!OidIsValid(joinOid))
			joinOid = LookupFuncName(joinName, 5, typeId, false);

		/* estimators must return float8 */
		if (get_func_rettype(joinOid) != FLOAT8OID)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
			 errmsg("join estimator function %s must return type \"float8\"",
					NameListToString(joinName))));

		/* Require EXECUTE rights for the estimator */
		aclresult = pg_proc_aclcheck(joinOid, GetUserId(), ACL_EXECUTE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_PROC,
						   NameListToString(joinName));
	}
	else
		joinOid = InvalidOid;

	/*
	 * now have OperatorCreate do all the work..
	 */
	OperatorCreate(oprName,		/* operator name */
				   oprNamespace,	/* namespace */
				   typeId1,		/* left type id */
				   typeId2,		/* right type id */
				   functionOid, /* function for operator */
				   commutatorName,		/* optional commutator operator name */
				   negatorName, /* optional negator operator name */
				   restrictionOid,		/* optional restrict. sel. procedure */
				   joinOid,		/* optional join sel. procedure name */
				   canMerge,	/* operator merges */
				   canHash, 	/* operator hashes */
				   cmd);
}


/*
 * Guts of operator deletion.
 */
void
RemoveOperatorById(Oid operOid)
{
	Relation	relation;
	HeapTuple	tup;

	relation = heap_open(OperatorRelationId, RowExclusiveLock);

	tup = SearchSysCache1(OPEROID, ObjectIdGetDatum(operOid));
	if (!HeapTupleIsValid(tup)) /* should not happen */
		elog(ERROR, "cache lookup failed for operator %u", operOid);

	simple_heap_delete(relation, &tup->t_self);

	ReleaseSysCache(tup);

	heap_close(relation, RowExclusiveLock);
}

void
AlterOperatorOwner_oid(Oid operOid, Oid newOwnerId)
{
	Relation	rel;

	rel = heap_open(OperatorRelationId, RowExclusiveLock);

	AlterOperatorOwner_internal(rel, operOid, newOwnerId, NULL);

	heap_close(rel, NoLock);
}

/*
 * change operator owner
 */
void
AlterOperatorOwner(List *name, TypeName *typeName1, TypeName *typeName2,
				   Oid newOwnerId, CommandContext cmd)
{
	Oid			operOid;
	Relation	rel;

	rel = heap_open(OperatorRelationId, RowExclusiveLock);

	operOid = LookupOperNameTypeNames(NULL, name,
									  typeName1, typeName2,
									  false, -1);

	AlterOperatorOwner_internal(rel, operOid, newOwnerId, cmd);

	heap_close(rel, NoLock);
}

static void
AlterOperatorOwner_internal(Relation rel, Oid operOid, Oid newOwnerId,
							CommandContext cmd)
{
	HeapTuple	tup;
	AclResult	aclresult;
	Form_pg_operator oprForm;

	Assert(RelationGetRelid(rel) == OperatorRelationId);

	tup = SearchSysCacheCopy1(OPEROID, ObjectIdGetDatum(operOid));
	if (!HeapTupleIsValid(tup)) /* should not happen */
		elog(ERROR, "cache lookup failed for operator %u", operOid);

	oprForm = (Form_pg_operator) GETSTRUCT(tup);

	/*
	 * If the new owner is the same as the existing owner, consider the
	 * command to have succeeded.  This is for dump restoration purposes.
	 */
	if (oprForm->oprowner != newOwnerId)
	{
		/* Superusers can always do it */
		if (!superuser())
		{
			/* Otherwise, must be owner of the existing object */
			if (!pg_oper_ownercheck(operOid, GetUserId()))
				aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_OPER,
							   NameStr(oprForm->oprname));

			/* Must be able to become new owner */
			check_is_member_of_role(GetUserId(), newOwnerId);

			/* New owner must have CREATE privilege on namespace */
			aclresult = pg_namespace_aclcheck(oprForm->oprnamespace,
											  newOwnerId,
											  ACL_CREATE);
			if (aclresult != ACLCHECK_OK)
				aclcheck_error(aclresult, ACL_KIND_NAMESPACE,
							   get_namespace_name(oprForm->oprnamespace));
		}

		/* Call BEFORE ALTER OPERATOR triggers */
		if (cmd!=NULL && (cmd->before != NIL || cmd->after != NIL))
		{
			cmd->objectId = operOid;
			cmd->objectname = NameStr(oprForm->oprname);
			cmd->schemaname = get_namespace_name(oprForm->oprnamespace);

			ExecBeforeCommandTriggers(cmd);
		}

		/*
		 * Modify the owner --- okay to scribble on tup because it's a copy
		 */
		oprForm->oprowner = newOwnerId;

		simple_heap_update(rel, &tup->t_self, tup);

		CatalogUpdateIndexes(rel, tup);

		/* Update owner dependency reference */
		changeDependencyOnOwner(OperatorRelationId, operOid, newOwnerId);
	}

	heap_freetuple(tup);

	/* Call AFTER ALTER OPERATOR triggers */
	if (cmd!=NULL && cmd->after != NIL)
		ExecAfterCommandTriggers(cmd);
}

/*
 * Execute ALTER OPERATOR SET SCHEMA
 */
void
AlterOperatorNamespace(List *names, List *argtypes, const char *newschema,
					   CommandContext cmd)
{
	List	   *operatorName = names;
	TypeName   *typeName1 = (TypeName *) linitial(argtypes);
	TypeName   *typeName2 = (TypeName *) lsecond(argtypes);
	Oid			operOid,
				nspOid;
	Relation	rel;

	rel = heap_open(OperatorRelationId, RowExclusiveLock);

	Assert(list_length(argtypes) == 2);
	operOid = LookupOperNameTypeNames(NULL, operatorName,
									  typeName1, typeName2,
									  false, -1);

	/* get schema OID */
	nspOid = LookupCreationNamespace(newschema);

	AlterObjectNamespace(rel, OPEROID, -1,
						 operOid, nspOid,
						 Anum_pg_operator_oprname,
						 Anum_pg_operator_oprnamespace,
						 Anum_pg_operator_oprowner,
						 ACL_KIND_OPER, cmd);

	heap_close(rel, RowExclusiveLock);
}

Oid
AlterOperatorNamespace_oid(Oid operOid, Oid newNspOid)
{
	Oid			oldNspOid;
	Relation	rel;

	rel = heap_open(OperatorRelationId, RowExclusiveLock);

	oldNspOid = AlterObjectNamespace(rel, OPEROID, -1,
									 operOid, newNspOid,
									 Anum_pg_operator_oprname,
									 Anum_pg_operator_oprnamespace,
									 Anum_pg_operator_oprowner,
									 ACL_KIND_OPER, NULL);

	heap_close(rel, RowExclusiveLock);

	return oldNspOid;
}
