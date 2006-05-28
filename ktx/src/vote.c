/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *  $Id: vote.c,v 1.14 2006/05/28 03:44:28 qqshka Exp $
 */

// vote.c: election functions by rc\sturm

#include "g_local.h"

void	BeginPicking();
void	BecomeCaptain(gedict_t *p);

// AbortElect is used to terminate the voting
// Important if player to be elected disconnects or levelchange happens
void AbortElect()
{
    int from;
	gedict_t *p;

	for( from = 0, p = world; (p = find_plrspc(p, &from)); ) {
		if ( p->v.elect_type != etNone ) {
			if ( is_elected(p, etCaptain) )
				k_captains = floor( k_captains );

			p->v.elect_type = etNone;
			p->v.elect_block_till = g_globalvars.time + 30; // block election for some time
		}
	}

	vote_clear( OV_ELECT ); // clear vote

// Kill timeout checker entity
	for( p = world; (p = find(p, FOFCLSN, "electguard")); ) 
		ent_remove( p );
}

void ElectThink()
{
	G_bprint(2, "The voting has timed out.\n"
				"Election aborted\n");
	self->s.v.nextthink = -1;

	AbortElect();
}

void VoteYes()
{
	int votes;

	if( !get_votes( OV_ELECT ) )
		return;

	if( self->v.elect_type != etNone ) {
		G_sprint(self, 2, "You cannot vote for yourself\n");
		return;
	}

	if( self->v.elect ) {
		G_sprint(self, 2, "--- your vote is still good ---\n");
		return;
	}

// register the vote
	self->v.elect = 1;

	G_bprint(2, "%s gives %s vote\n", self->s.v.netname, g_his( self ));

// calculate how many more votes are needed
	if ( (votes = get_votes_req( OV_ELECT, true )) )
		G_bprint(2, "\x90%d\x91 more vote%s needed\n", votes, count_s( votes ));

	vote_check_elect ();
}

void VoteNo()
{
	int votes;

// withdraw one's vote
	if( !get_votes( OV_ELECT ) || self->v.elect_type != etNone || !self->v.elect )
		return;

// unregister the vote
	self->v.elect = 0;

	G_bprint(2, "%s withdraws %s vote\n", self->s.v.netname, g_his( self ));

// calculate how many more votes are needed
	if ( (votes = get_votes_req( OV_ELECT, true )) )
		G_bprint(2, "\x90%d\x91 more vote%s needed\n", votes, count_s( votes ));

	vote_check_elect ();
}

// get count of particular votes

int get_votes( int fofs )
{
	int from;
	int votes = 0;
	gedict_t *p;

	for ( from = 0, p = world; (p = find_plrspc(p, &from)); )
		if ( *(int*)((byte*)(&p->v)+fofs) )
			votes++;

	return votes;
}

// get count of particular votes and filter by value

int get_votes_by_value( int fofs, int value )
{
	int from;
	int votes = 0;
	gedict_t *p;

	for ( from = 0, p = world; (p = find_plrspc(p, &from)); )
		if ( *((int*)(&(p->v)+fofs)) == value )
			votes++;

	return votes;
}

int get_votes_req( int fofs, qboolean diff )
{
	float percent = 51;
	int   votes, vt_req, idx, el_type;

	votes   = get_votes( fofs );

	switch ( fofs ) {
		case OV_BREAK:   percent = cvar("k_vp_break");  break;
		case OV_PICKUP:  percent = cvar("k_vp_pickup"); break;
		case OV_RPICKUP: percent = cvar("k_vp_rpickup"); break;
		case OV_MAP:
					    percent = cvar("k_vp_map");
						idx = vote_get_maps ();
						if ( idx >= 0 && !strnull( GetMapName(maps_voted[idx].map_id) ) )
							votes = maps_voted[idx].map_votes;
						else
							votes = 0;
						break;
		case OV_ELECT:
						if ( (el_type = get_elect_type ()) == etAdmin ) {
							percent = cvar("k_vp_admin");
							break;
						}
						else if ( el_type == etCaptain ) {
							percent = cvar("k_vp_captain");
							break;
						}
						else {
							percent = 100; break; // unknown/none election
							break;
						}
	}

	percent = bound(0.51, bound(51, percent, 100)/100, 1); // calc and bound percentage between 50% to 100%

	vt_req  = ceil( percent * CountPlayers() );

	if ( fofs == OV_ELECT )
		vt_req = max(2, vt_req); // if election, at least 2 votes needed
	else if ( fofs == OV_BREAK && k_matchLess && match_in_progress == 1 )
		vt_req = max(2, vt_req); // at least 2 votes in this case
	else if ( fofs == OV_BREAK )
		vt_req = max(1, vt_req); // at least 1 vote in any case
	else if ( fofs == OV_RPICKUP )
		vt_req = max(3, vt_req); // at least 3 votes in this case

	if ( diff )
		return max(0, vt_req - votes);

	return max(0, vt_req);
}

int is_admins_vote( int fofs )
{
	int from;
	int votes = 0;
	gedict_t *p;

	for ( from = 0, p = world; (p = find_plrspc(p, &from)); )
		if ( *(int*)((byte*)(&p->v)+fofs) && is_adm( p ) )
			votes++;

	return votes;
}

void vote_clear( int fofs )
{
	int from;
	gedict_t *p;

	for ( from = 0, p = world; (p = find_plrspc(p, &from)); )
		*(int*)((byte*)(&p->v)+fofs) = 0;
}

// return true if player invoke one of particular election
qboolean is_elected(gedict_t *p, electType_t et)
{
	return (p->v.elect_type == et);
}

int get_elect_type ()
{
    int from;
	gedict_t *p;

	for( from = 0, p = world; (p = find_plrspc(p, &from)); ) {
		if( is_elected(p, etAdmin) ) // elected admin
			return etAdmin;

		if( is_elected(p, etCaptain) ) // elected captain
			return etCaptain;
	}

	return etNone;
}

char *get_elect_type_str ()
{

	switch ( get_elect_type () ) {
		case etNone: 	return "None";
		case etCaptain:	return "Captain";
		case etAdmin: 	return "Admin";
	}

	return "Unknown";
}


int     maps_voted_idx;

votemap_t maps_voted[MAX_CLIENTS];

// fill maps_voted[] with data,
// return the index in maps_voted[] of most voted map
// return -1 inf no votes at all or some failures
// if admin votes for map - map will be treated as most voted
int vote_get_maps ()
{
	int from;
	int best_idx = -1, i;
	gedict_t *p;

	memset(maps_voted, 0, sizeof(maps_voted));
	maps_voted_idx = -1;

	if ( !get_votes( OV_MAP ) )
		return -1; // no one votes at all

	for( from = 0, p = world; (p = find_plrspc(p, &from)); ) {

		if ( !p->v.map )
			continue; // player is not voted

		for ( i = 0; i < MAX_CLIENTS; i++ ) {
			if ( !maps_voted[i].map_id )
				break; // empty

			if ( maps_voted[i].map_id == p->v.map )
				break; // already count votes for this map
		}

		if ( i >= MAX_CLIENTS )
			continue; // heh, all slots is full, just for sanity

		maps_voted[i].map_id     = p->v.map;
		maps_voted[i].map_votes += 1;
		maps_voted[i].admins    += (is_adm( p ) ? 1 : 0);

		// find the most voted map
		if ( best_idx < 0 || maps_voted[i].map_votes > maps_voted[best_idx].map_votes )
			best_idx   = i;

		// admin voted maps have priority
		if ( maps_voted[i].admins > maps_voted[best_idx].admins )
			best_idx   = i;
	}

	return (maps_voted_idx = best_idx);
}

void vote_check_map ()
{
	int   vt_req = get_votes_req( OV_MAP, true );
	char  *m  = "";

	if ( maps_voted_idx < 0 || strnull( m = GetMapName(maps_voted[maps_voted_idx].map_id) ) )
		return;

	if ( !k_matchLess )
	if ( match_in_progress )
		return;

	if ( maps_voted[maps_voted_idx].admins )
		G_bprint(2, "%s\n", redtext("Admin veto"));
	else if( !vt_req  )
		G_bprint(2, "%s votes for mapchange.\n", redtext("Majority"));
	else
		return;

	vote_clear( OV_MAP );

	changelevel( m );
}

void vote_check_break ()
{
	if ( !match_in_progress || intermission_running )
		return;

	if( !get_votes_req( OV_BREAK, true ) ) {
		vote_clear( OV_BREAK );

		G_bprint(2, "%s\n", redtext("Match stopped by majority vote"));

		EndMatch( 1 );

		return;
	}
}

void vote_check_elect ()
{
	gedict_t *p;
	int   from;

	if( !get_votes_req( OV_ELECT, true ) ) {

		for( from = 0, p = world; (p = find_plrspc(p, &from)); )
			if ( p->v.elect_type != etNone )
				break;

		if ( !p ) { // nor admin nor captain found - probably bug
			AbortElect();
			return;
		}

		if( !(p->k_spectator && match_in_progress) )
		if( is_elected(p, etAdmin) ) // s: election was admin election
			BecomeAdmin(p, AF_ADMIN);

		if( !match_in_progress )
		if( is_elected(p, etCaptain) ) // s: election was captain election
			BecomeCaptain(p);

		AbortElect();
		return;
	}
}

// !!! do not confuse rpickup and pickup
void vote_check_pickup ()
{
	gedict_t *p;
	int veto;

	if ( match_in_progress || k_captains )
		return;

	if ( !get_votes( OV_PICKUP ) )
		return;

	veto = is_admins_vote( OV_PICKUP );

	if( veto || !get_votes_req( OV_PICKUP, true ) ) {
		vote_clear( OV_PICKUP );

		if ( veto )
			G_bprint(2, "console: admin veto for pickup\n");
		else
			G_bprint(2, "console: a pickup game it is then\n");

		for( p = world;	(p = find( p, FOFCLSN, "player" )); ) {

			stuffcmd(p, "break\n"
						"color 0\n"
						"team \"\"\n"
						"skin base\n");
		}

		return;
	}
}

// !!! do not confuse rpickup and pickup
void vote_check_rpickup ()
{
	float frnd;
    int i, tn, pl_cnt, pl_idx;
	gedict_t *p;
	int veto;


	if ( match_in_progress || k_captains )
		return;

	if ( !get_votes( OV_RPICKUP ) )
		return;

   	// Firstly obtain the number of players we have in total on server
   	pl_cnt = CountPlayers();

	if ( pl_cnt < 4 )
		return;

	veto = is_admins_vote( OV_RPICKUP );

	if( veto || !get_votes_req( OV_RPICKUP, true ) ) {
		vote_clear( OV_RPICKUP );

		for( p = world; (p = find(p, FOFCLSN, "player")); )
			p->k_teamnumber = 0;

		for( tn = 1; pl_cnt > 0; pl_cnt-- ) {
			frnd = g_random(); // bound is macros - so u _can't_ put g_random inside bound
			pl_idx = bound(0, (int)( frnd * pl_cnt ), pl_cnt-1 ); // select random player between 0 and pl_cnt

			for( i = 0, p = world; (p = find(p, FOFCLSN, "player")); ) {
				if ( p->k_teamnumber )
					continue;

				if ( i == pl_idx ) {
					p->k_teamnumber = tn;
					tn = (tn == 1 ? 2 : 1); // next random player will be in other team

            		if( p->k_teamnumber == 1 )
                		stuffcmd(p, "break\ncolor  4\nskin \"\"\nteam red\n");
            		else
                		stuffcmd(p, "break\ncolor 13\nskin \"\"\nteam blue\n");

					break;
				}

				i++;
			}
		}

		if ( veto )
			G_bprint(2, "console: admin veto for %s\n", redtext("random pickup"));
		else
    		G_bprint(2, "console: %s game it is then\n", redtext("random pickup"));

		return;
	}
}

void vote_check_all ()
{
	vote_check_map ();
	vote_check_break ();
	vote_check_elect ();
	vote_check_pickup ();
	vote_check_rpickup ();
}

