/************************************************************************
| $Id: cl_barbarian.cpp,v 1.33 2005/04/19 18:34:15 urizen Exp $
| cl_barbarian.C
| Description:  Commands for the barbarian class.
*/
#include <structs.h>
#include <player.h>
#include <levels.h>
#include <character.h>
#include <spells.h>
#include <utility.h>
#include <fight.h>
#include <mobile.h>
#include <magic.h>
#include <connect.h>
#include <handler.h>
#include <act.h>
#include <interp.h>
#include <returnvals.h>
#include <room.h>
#include <db.h>

extern struct index_data *obj_index;
extern bool str_prefix(const char *astr, const char *bstr);
extern CWorld world;
int attempt_move(CHAR_DATA *ch, int cmd, int is_retreat = 0);

int do_rage(struct char_data *ch, char *argument, int cmd)
{
  char_data *victim;
  char name[256];

  if(GET_HIT(ch) == 1) {
    send_to_char("You are feeling too weak right now to work yourself up into "
                 "a rage.", ch);
    return eFAILURE;
  }

  if(IS_MOB(ch) || GET_LEVEL(ch) >= ARCHANGEL )
    ;
  else if(!has_skill(ch, SKILL_RAGE)) {
    send_to_char("You should learn the skill before you try doing any raging in this machine...\r\n", ch);
    return eFAILURE;
  }

  one_argument(argument, name);

  if(!(victim = get_char_room_vis(ch, name))) {
    if(ch->fighting)
      victim = ch->fighting;
    else {
      send_to_char( "Who do you want to rage on?\n\r", ch );
      return eFAILURE;
    }
  }

  if(ch->in_room != victim->in_room) {
    send_to_char("That person seems to have left.\n\r", ch);
    return eFAILURE;
  }
 
  if (victim == ch) {
    send_to_char("Aren't we funny today...\n\r", ch);
    return eFAILURE;
  }

  if(!can_attack(ch) || !can_be_attacked(ch, victim))
    return eFAILURE;

  if (!skill_success(ch,victim,SKILL_RAGE)) {
    act ("You start advancing towards $N, but trip over your own feet!", 
         ch, 0, victim, TO_CHAR, 0);
    act ("$n starts advancing toward you, but trips over $s own feet!",
         ch, 0, victim, TO_VICT, 0);
    act ("$n starts advancing towards $N, but trips over $s own feet!",
         ch, 0, victim, TO_ROOM, NOTVICT);
     
    GET_POS(ch) = POSITION_SITTING;
    SET_BIT(ch->combat, COMBAT_BASH1);
  }
  else {
    act ("You advance confidently towards $N, and fly into a rage!",
        ch, 0, victim, TO_CHAR, 0);
    act ("$n advances confidently towards you, and flies into a rage!",
        ch, 0, victim, TO_VICT, 0);
    act ("$n advances confidently towards $N, and flies into a rage!",
            ch, 0, victim, TO_ROOM, NOTVICT);

    SET_BIT(ch->combat, COMBAT_RAGE1);
  }

  WAIT_STATE(ch, PULSE_VIOLENCE * 3);

  if(!ch->fighting)
     return attack(ch, victim, TYPE_UNDEFINED);

  // chance of bonus round at high level of skill
  if(has_skill(ch,SKILL_RAGE) > 75 && !number(0, 9))
     return attack(ch, victim, TYPE_UNDEFINED);

  return eSUCCESS;
}

int do_battlecry(struct char_data *ch, char *argument, int cmd)
{
  struct follow_type *f;
  if(IS_MOB(ch) || GET_LEVEL(ch) >= ARCHANGEL)
    ;
  else if(!has_skill(ch, SKILL_BATTLECRY)) {
    send_to_char("Have to learn how to battlecry before you can run with the big boys...\r\n", ch);
    return eFAILURE;
  }

  if (!ch->fighting) {
     send_to_char("You must be fighting already in order to battlecry.\n\r", ch);
     return eFAILURE;
  }

  if (!skill_success(ch,NULL,SKILL_BATTLECRY)) {
     act ("You give a cry of defiance, but trip over your own feet!", ch, 0, 0, TO_CHAR, 0);
     act ("$n gives a cry of defiance, but trips over $s own feet!", ch, 0, 0, TO_ROOM, 0);
     
     GET_POS(ch) = POSITION_SITTING;
     SET_BIT(ch->combat, COMBAT_BASH1);
  }
  else {
     act ("You give a battlecry, sounding your defiance!", ch, 0, 0, TO_CHAR, 0);
     act ("$n yells 'They can take our lives, but they'll never take OUR FREEDOM!'", ch, 0, 0, TO_ROOM, 0);

     SET_BIT(ch->combat, COMBAT_RAGE1);
     if (ch->followers) f = ch->followers;
     else if (ch->master) 
     {
	f = ch->master->followers;
        act ("You give a battlecry, sounding your defiance!", ch->master, 0, 0, TO_CHAR, 0);
        act ("$n gives a loud cry of agreement!", ch->master, 0, 0, TO_ROOM, 0);
        SET_BIT(f->follower->combat, COMBAT_RAGE1);

     }
     else f = NULL;

     for(; f; f = f->next) {
        if (!IS_AFFECTED(f->follower, AFF_GROUP) ||
	    (IS_SET(f->follower->combat, COMBAT_RAGE1)) ||
	    (IS_SET(f->follower->combat, COMBAT_RAGE2)) ||
	    (IS_SET(f->follower->combat, COMBAT_BERSERK)) ||
	    (f->follower->in_room != ch->in_room) ||
            (!f->follower->fighting))
           continue;
	if (f->follower == ch) continue;
        act ("You give a battlecry, sounding your defiance!", f->follower, 0, 0, TO_CHAR, 0);
        act ("$n gives a loud cry of agreement!", f->follower, 0, 0, TO_ROOM, 0);
        SET_BIT(f->follower->combat, COMBAT_RAGE1);
	}
     }

   if(has_skill(ch,SKILL_BATTLECRY) > 40 && !number(0, 4))
      WAIT_STATE(ch, PULSE_VIOLENCE * 2);
   else WAIT_STATE(ch, PULSE_VIOLENCE * 3);
   return eSUCCESS;
}


int do_berserk(struct char_data *ch, char *argument, int cmd)
{
  struct char_data *victim;
  char name[256];
  int bSuccess = 0;
  int retval = 0;
  
  if(IS_MOB(ch))
    ;
  else if(!has_skill(ch, SKILL_BERSERK)) {
    send_to_char("You aren't crazy enough for that yet... try rage maybe...\r\n", ch);
    return eFAILURE;
  }

  if(GET_HIT(ch) == 1) {
    send_to_char("You are feeling too weak right now to work yourself up into "
                 "a frenzy.", ch);
    return eFAILURE;
  }

  one_argument(argument, name);

  if(!(victim = get_char_room_vis(ch, name))) {
    if(ch->fighting)
      victim = ch->fighting;
    else {
      send_to_char( "Who do you want to go berserk on?\n\r", ch );
      return eFAILURE;
    }
  }

  if(ch->in_room != victim->in_room) {
    send_to_char("That person seems to have left.\n\r", ch);
    return eFAILURE;
  }
 
  if (victim == ch) {
    send_to_char("Aren't we funny today...\n\r", ch);
    return eFAILURE;
  }

  if(!can_attack(ch) || !can_be_attacked(ch, victim))
    return eFAILURE;

  if (!skill_success(ch,victim,SKILL_BERSERK)) {
    act ("You start freaking out on $N, but trip over your own feet!", ch, 0, victim, TO_CHAR, 0);
    act ("$n starts freaking out on you, but trips over $s own feet!", ch, 0, victim, TO_VICT, 0);
    act ("$n starts freaking out on $N, but trips over $s own feet!", ch, 0, victim, TO_ROOM, NOTVICT);
    GET_POS(ch) = POSITION_SITTING;
    if(has_skill(ch,SKILL_BERSERK) > 50 && !number(0, 5)) {
       SET_BIT(ch->combat, COMBAT_BASH2);
       send_to_char("Your advanced training in berserk allows you to roll with your fall and get up faster.\r\n", ch);
       WAIT_STATE(ch, PULSE_VIOLENCE * 2);
    }
    else {
       SET_BIT(ch->combat, COMBAT_BASH1);     
       WAIT_STATE(ch, PULSE_VIOLENCE * 3);
    }
  }
  else {
    act ("You start FOAMING at the mouth, and you go BERSERK on $N!", ch, 0, victim, TO_CHAR, 0);
    act ("$n starts FOAMING at the mouth, and goes BERSERK on you!", ch, 0, victim, TO_VICT, 0);
    act ("$n starts FOAMING at the mouth, as $e goes BERSERK on $N!", ch, 0, victim, TO_ROOM, NOTVICT);
    bSuccess = 1;
  }

  if (bSuccess && !IS_SET(ch->combat, COMBAT_RAGE1))
     SET_BIT(ch->combat, COMBAT_RAGE1);

  if(!ch->fighting)
     retval = attack(ch, victim, TYPE_UNDEFINED);

  if(!IS_SET(retval, eCH_DIED)) {
   if (!IS_SET(retval, eVICT_DIED) && !IS_NPC(ch) && IS_SET(ch->pcdata->toggles, PLR_WIMPY))
    WAIT_STATE(ch, PULSE_VIOLENCE * 2);
  
    REMOVE_BIT(ch->combat, COMBAT_RAGE1);
    REMOVE_BIT(ch->combat, COMBAT_RAGE2);
     
    // I set the berserk bit AFTER the attack, to reduce the advantage
    // a bit.
    if (bSuccess) {
       SET_BIT(ch->combat, COMBAT_BERSERK);
       if(!IS_NPC(ch))
          GET_AC(ch) += 30; // we do this here, so we know if someone dies with COMBAT_BERSERK to 
                            // give them their AC back
    }
  }
  return retval;
}


int do_headbutt(struct char_data *ch, char *argument, int cmd)
{
  struct char_data *victim;
  char name[256];
  int retval;

  if(IS_MOB(ch) || GET_LEVEL(ch) >= ARCHANGEL)
    ;
  else if(!has_skill(ch, SKILL_SHOCK)) {
    send_to_char("You'd bonk yourself silly without proper training.\r\n", ch);
    return eFAILURE;
  }

  one_argument(argument, name);

  victim = get_char_room_vis( ch, name );
  if ( victim == NULL )
    victim = ch->fighting;

  if ( victim == NULL ) {
    send_to_char( "Headbutt whom?\n\r", ch );
    return eFAILURE;
  }

  if (victim == ch) {
    send_to_char("Aren't we funny today...\n\r", ch);
    return eFAILURE;
  }

  if(!can_attack(ch) || !can_be_attacked(ch, victim))
    return eFAILURE;

  if(IS_MOB(victim) && IS_SET(victim->mobdata->actflags, ACT_HUGE)) {
    send_to_char("You're to puny too headbutt someone that HUGE!\n\r",ch);
    return eFAILURE;
  }

  if (IS_SET(victim->combat, COMBAT_BERSERK) && (IS_NPC(victim) || has_skill(victim, SKILL_BERSERK) > 80))
  {
     act("In your enraged state, you shake off $n's attempt to immobilize you.", ch, NULL, victim, TO_VICT, 
0);
     act("$N shakes off $n's attempt to immobilize them.",ch, NULL, victim, TO_ROOM, NOTVICT);
     act("$N shakes off your attempt to immobilize them.",ch, NULL, victim, TO_ROOM, NOTVICT);
     WAIT_STATE(ch, PULSE_VIOLENCE*4);
        return eSUCCESS;
  }
  
  if (!skill_success(ch,victim,SKILL_SHOCK) ) 
  {
//    act( "$n tries to headbutt you but fails miserably.", ch, NULL, 
//victim, TO_VICT , 0);
//    act( "You try to headbutt $N, but fail miserably.", ch, NULL, 
//victim, TO_CHAR , 0);
//    act( "$n tries to headbutt $N, but fails miserably.", ch, NULL, 
//victim, TO_ROOM, NOTVICT );
    if(has_skill(ch,SKILL_SHOCK) > 60 && !number(0, 3)) {
       send_to_char("With your advanced knowledge of headbutt, you recover more quickly.\r\n", ch);
       WAIT_STATE(ch, PULSE_VIOLENCE*2);
    }
    else WAIT_STATE(ch, PULSE_VIOLENCE*3);
    retval = damage (ch, victim, 0, TYPE_UNDEFINED, SKILL_SHOCK, 0);
    // the damage call here takes care of starting combat and such
    retval = eSUCCESS;
  }
  else {
    //act( "$n slams $s forehead into your face! You are SHOCKED!", ch, 
//NULL, victim, TO_VICT , 0);
  //  act( "You slam your forehead into $N's face! $N looks SHOCKED!", ch, 
//NULL, victim, TO_CHAR , 0);
//    act( "$n slams $s forehead into $N's face! $N looks SHOCKED!", ch, 
//NULL, victim, TO_ROOM, NOTVICT );

    if (IS_SET(victim->combat, COMBAT_BERSERK))
       REMOVE_BIT(victim->combat, COMBAT_BERSERK);

    set_fighting(victim, ch);
    WAIT_STATE(ch, PULSE_VIOLENCE*4);
    WAIT_STATE(victim, PULSE_VIOLENCE*2);
    SET_BIT(victim->combat, COMBAT_SHOCKED);
    retval = damage (ch, victim, 50, TYPE_HIT, SKILL_SHOCK, 0);
    if (!SOMEONE_DIED(retval) && !number(0,9) &&
	  ch->equipment[WEAR_HEAD] && obj_index[ch->equipment[WEAR_HEAD]->item_number].virt == 508)
    {
	act("$n's spiked helmet crackles as it strikes $N's face!", ch, NULL, victim, TO_ROOM,NOTVICT);
	act("$n's spiked helmet crackles as it strikes your face!", ch, NULL, victim, TO_VICT, 0);
	act("Your spiked helmet crackles as it strikes $N's face!", ch, NULL, victim, TO_CHAR, 0);
//	retval = damage(ch, victim, 50, TYPE_PIERCE, TYPE_UNDEFINED, 0);
	retval = spell_shocking_grasp(50,ch,victim,0,60);
	// TWEAKME
    }
  }

  return retval;
}

int do_bloodfury(struct char_data *ch, char *argument, int cmd)
{
  struct affected_type af;
  float modifier;
  int duration = 42;
  if(IS_MOB(ch) || GET_LEVEL(ch) >= ARCHANGEL)
    ;
  else if(!has_skill(ch, SKILL_BLOOD_FURY)) {
    send_to_char("You've no idea how to raise such bloodlust.\r\n", ch);
    return eFAILURE;
  }

  if(affected_by_spell(ch, SKILL_BLOOD_FURY)) {
    send_to_char("Your body can not yet take the strain of another blood fury yet.\r\n", ch);
    return eFAILURE;
  }

  if (!skill_success(ch,NULL,SKILL_BLOOD_FURY)) 
  {
    act("$n starts breathing heavily, then chokes and tries to clear $s head.", ch, NULL, NULL, TO_ROOM, NOTVICT);
    send_to_char("You try to pysch yourself up and choke on the taste of blood.\r\n", ch);
    duration = 42 - (GET_LEVEL(ch) / 2);
  }
  else 
  {
    act( "Panting heavily $n's eyes glaze red $e begins to move with renewed fury!",
	  ch, NULL, NULL, TO_ROOM, NOTVICT );
    send_to_char("Your sight tinges red with the blood of battle and your " 
                 "limbs feel strong with death and destruction deep within " 
                 "your bones.\r\n", ch);

    modifier = .2 + (.00375 * has_skill(ch,SKILL_BLOOD_FURY));  // mod = .2 - .5

    GET_HIT(ch) += (int)((float)GET_MAX_HIT(ch) * modifier);
    if(GET_HIT(ch) > GET_MAX_HIT(ch))
      GET_HIT(ch) = GET_MAX_HIT(ch);
    duration = 42 - (GET_LEVEL(ch) / 4);
  }

  af.type      = SKILL_BLOOD_FURY;
  af.duration  = duration;
  af.modifier  = 0;
  af.location  = 0;
  af.bitvector = 0;

  affect_to_char(ch, &af);

  return eSUCCESS;
}

int do_crazedassault(struct char_data *ch, char *argument, int cmd)
{
  struct affected_type af;
  int duration = 20;
  if(affected_by_spell(ch, SKILL_CRAZED_ASSAULT) && GET_LEVEL(ch) < IMMORTAL) {
    send_to_char("Your body is still recovering from your last crazed assault technique.\r\n", ch);
    return eFAILURE; 
  }

  if(IS_MOB(ch))
    ;
  else if(!has_skill(ch, SKILL_CRAZED_ASSAULT)) {
    send_to_char("You just aren't crazy enough...try assaulting old ladies.\r\n", ch);
    return eFAILURE;
  }
          
  if(!skill_success(ch,NULL,SKILL_CRAZED_ASSAULT)) {
    send_to_char("You try to psyche yourself up for it but just can't muster the concentration.\r\n", ch);
    duration = 8 - has_skill(ch, SKILL_CRAZED_ASSAULT) / 10;
  }
  else {
    send_to_char("Your mind focuses completely on hitting your opponent.\r\n", ch);
    af.type = SKILL_CRAZED_ASSAULT;
    af.duration  = 2;
    af.modifier  = (has_skill(ch,SKILL_CRAZED_ASSAULT) / 5) + 7; 
    af.location  = APPLY_HITROLL;
    af.bitvector = 0;
    affect_to_char(ch, &af);
    duration = 16 - has_skill(ch, SKILL_CRAZED_ASSAULT) / 10;
  }
  
  WAIT_STATE(ch, PULSE_VIOLENCE);
  
  af.type = SKILL_CRAZED_ASSAULT;
  af.duration  = duration;
  af.modifier  = 0; 
  af.location  = APPLY_NONE;
  af.bitvector = 0;
  affect_to_char(ch, &af);
  return eSUCCESS;
} 

int do_bullrush(struct char_data *ch, char *argument, int cmd)
{
  char name[MAX_INPUT_LENGTH];
  char who[MAX_INPUT_LENGTH];
  int dir = 0;
  int retval;
  char_data *victim;
  extern char *dirs[];

  if(GET_HIT(ch) == 1) {
    send_to_char("You are feeling too weak right now for rushing to and fro.\r\n", ch);
    return eFAILURE;
  }
 
  if(IS_MOB(ch) || GET_LEVEL(ch) >= ARCHANGEL )
    ;
  else if(!has_skill(ch, SKILL_BULLRUSH)) {
    send_to_char("Closest yer gonna get to a bull right now is a Red one..and you have to drink it...\r\n", ch);          
    return eFAILURE;
  }   
    
  argument = one_argument(argument, name);
  one_argument(argument, who);
  if(!*name) {
     send_to_char("Bullrush which direction?\r\n", ch);
     return eFAILURE;
  }
  if (!*who)
  {
     send_to_char("Bullrush on.. who?\r\n",ch);
     return eFAILURE;
  }

  for(int i = 0; i < 6; i++) {
    if(!str_prefix(name,dirs[i]))
    {
      dir = i + 1;
      break;
    }
  }
  
  if(!dir) {
    send_to_char("Bullrush a valid direction dumb barb...like north maybe?\r\n", ch);
    return eFAILURE;
  }

  // before we move anyone, we need to check for any spec procs in the
  // room like guild guards
  retval = special( ch, dir, "" );
  if(IS_SET(retval, eSUCCESS) || IS_SET(retval, eCH_DIED))
     return retval;

  retval = attempt_move(ch, dir);
  if(SOMEONE_DIED(retval))
    return retval;
  if (!(victim = get_char_room_vis(ch,who)))
  {
     send_to_char("You charge in, but is left confused by the complete lack of such a target!\r\n",ch);
     WAIT_STATE(ch,PULSE_VIOLENCE/2);
     return eFAILURE;
  }
  WAIT_STATE(ch, PULSE_VIOLENCE);

  if(!skill_success(ch,victim,SKILL_BULLRUSH) )
  {
    send_to_char("You rush in madly and fail to find your target!\r\n", ch);
    act( "$n rushes into the room with nostrils flaring then looks around sheepishly.",
	  ch, NULL, NULL, TO_ROOM, NOTVICT );
    return eFAILURE;
  }

  if(!victim || victim == ch) {
    send_to_char("You successfully rush in and bushwack....the air.\r\n", ch);
    return eFAILURE;
  }

  act( "$n rushes into the room with an amazingly violent speed!",
       ch, NULL, NULL, TO_ROOM, NOTVICT );
  return attack(ch, victim, TYPE_UNDEFINED);
}

int do_ferocity(struct char_data *ch, char *argument, int cmd)
{
  struct affected_type af;

  if(IS_MOB(ch) || GET_LEVEL(ch) >= ARCHANGEL)
    ;
  else if(!has_skill(ch, SKILL_FEROCITY)) {
    send_to_char("You're just not angry enough!\r\n", ch);
    return eFAILURE;
  }

  if(!IS_AFFECTED(ch, AFF_GROUP)) {
    send_to_char("You have no group to inspire.\r\n", ch);
    return eFAILURE;
  }

  if (skill_success(ch,NULL,SKILL_FEROCITY)) {
     send_to_char("Guess you just weren't that angry.\r\n", ch);
     act ("$n tries to rile you up but just seems to be pouty.", ch, 0, 0, TO_ROOM, 0);
  }
  else {
    act ("$n lets out a deafening roar!", ch, 0, 0, TO_ROOM, 0);
    send_to_char("Your heart beats adrenaline though your body and you roar with ferocity!\r\n", ch);

    for(char_data * tmp_char = world[ch->in_room].people; tmp_char; tmp_char = tmp_char->next_in_room)
    {
      if(tmp_char == ch)
        continue;
      if(!ARE_GROUPED(ch, tmp_char))
        continue;

      affect_from_char(tmp_char, SKILL_FEROCITY);
      affect_from_char(tmp_char, SKILL_FEROCITY);
      act("$n's fierce roar gets your adrenaline pumping!", ch, 0, tmp_char, TO_VICT, 0);

      af.type      = SKILL_FEROCITY;
      af.duration  = 1 + has_skill(ch,SKILL_FEROCITY) / 10;
      af.modifier  = 50;
      af.location  = APPLY_HIT;
      af.bitvector = 0;
      affect_to_char(tmp_char, &af);
      af.modifier  = 1;
      af.location  = APPLY_HP_REGEN;
      affect_to_char(tmp_char, &af);
    }
  }

  WAIT_STATE(ch, PULSE_VIOLENCE * 2);
  GET_MOVE(ch) /= 2;
  return eSUCCESS;
}

void barb_magic_resist(char_data *ch, int old, int nw)
{
  int bonus=0,i;
  int oldbonus = (old / 10) +1;
  bonus = (nw/10+1) - oldbonus;
  if (bonus)
    for (i = 0; i <= SAVE_TYPE_MAX; i++)
      ch->saves[i] += bonus;

}
