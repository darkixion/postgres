/*-------------------------------------------------------------------------
 *
 * pg_event_trigger.h
 *	  definition of the system "event trigger" relation (pg_event_trigger)
 *	  along with the relation's initial contents.
 *
 *
 * Portions Copyright (c) 1996-2011, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/catalog/pg_event_trigger.h
 *
 * NOTES
 *	  the genbki.pl script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_EVENT_TRIGGER_H
#define PG_EVENT_TRIGGER_H

#include "catalog/genbki.h"

/* ----------------
 *		pg_event_trigger definition.	cpp turns this into
 *		typedef struct FormData_pg_event_trigger
 * ----------------
 */
#define EventTriggerRelationId  3466

CATALOG(pg_event_trigger,3466)
{
	NameData	evtname;		/* trigger's name */
	NameData    evtevent;		/* trigger's event */
	Oid			evtfoid;		/* OID of function to be called */
	char		evttype;		/* BEFORE/INSTEAD OF */
	char		evtenabled;		/* trigger's firing configuration WRT
								 * session_replication_role */
#ifdef CATALOG_VARLEN
	int2        evttags[1];		/* command TAGs this event trigger targets */
#endif
} FormData_pg_event_trigger;

/* ----------------
 *		Form_pg_event_trigger corresponds to a pointer to a tuple with
 *		the format of pg_event_trigger relation.
 * ----------------
 */
typedef FormData_pg_event_trigger *Form_pg_event_trigger;

/* ----------------
 *		compiler constants for pg_event_trigger
 * ----------------
 */
#define Natts_pg_event_trigger					6
#define Anum_pg_event_trigger_evtname			1
#define Anum_pg_event_trigger_evtevent			2
#define Anum_pg_event_trigger_evtfoid			3
#define Anum_pg_event_trigger_evttype			4
#define Anum_pg_event_trigger_evtenabled		5
#define Anum_pg_event_trigger_evttags			6

/*
 * Times at which an event trigger can be fired. These are the
 * possible values for pg_event_trigger.evttype.
 *
 * pg_trigger is using binary mask tricks to make it super fast, but we don't
 * need to be that tricky here: we're talking about commands, not data editing,
 * and we don't have so many conditions, only type and enabled.
 */
#define EVTG_FIRED_BEFORE			'B'
#define EVTG_FIRED_INSTEAD_OF		'I'

#endif   /* PG_EVENT_TRIGGER_H */
