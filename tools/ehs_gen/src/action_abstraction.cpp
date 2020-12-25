#include <assert.h>
#include <iostream>
#include <vector>
#include "defs.hpp"
#include "action_abstraction.hpp"

ActionAbstraction::~ActionAbstraction(){}

action_c NullActionAbstraction::get_actions(const Game *game,
                                            const State &state) {
  action_c actions(MAX_ABSTRACT_ACTIONS);
  int num_actions = 0;
  bool error = false;
  for (int a = 0; a < NUM_ACTION_TYPES; ++a) {
    Action action;
    action.type = (ActionType)a;
    action.size = 0;
    if (action.type == a_raise) {
      int32_t min_raise_size;
      int32_t max_raise_size;
      if (raiseIsValid(game, &state, &min_raise_size, &max_raise_size)) {
        if (num_actions + (max_raise_size - min_raise_size + 1) >
            MAX_ABSTRACT_ACTIONS) {
          error = true;
          break;
        }
        for (int s = min_raise_size; s <= max_raise_size; ++s) {
          actions[num_actions] = action;
          actions[num_actions].size = s;
          ++num_actions;
        }
      }
    } else if (isValidAction(game, &state, 0, &action)) {
      /* If you hit this assert, there are too many abstract actions allowed.
       * Either coarsen the betting abstraction or increase
       * MAX_ABSTRACT_ACTIONS
       * in constants.hpp
       */
      if (num_actions >= MAX_ABSTRACT_ACTIONS) {
        error = true;
        break;
      }
      actions[num_actions] = action;
      ++num_actions;
    }
  }

  actions.resize(num_actions);
  //std::cout << "actions: " << std::endl;
  //for(unsigned i = 0; i < actions.size(); ++i)
      //std::cout << ActionsStr[actions[i].type] << " ";
  //std::cout << "" << std::endl;

  /* If you hit this assert, there are too many abstract actions allowed.
   * Either coarsen the betting abstraction or increase MAX_ABSTRACT_ACTIONS
   * in constants.hpp
   */
   assert( !error );

  return actions;
}

PotRelationActionAbstraction::PotRelationActionAbstraction(
    const Game *game, std::vector<double> fractions)
    : game(game), fractions(fractions) {
    
    
    }

action_c PotRelationActionAbstraction::get_actions(const Game *game,
                                                   const State &state){
  action_c actions(MAX_ABSTRACT_ACTIONS);
  int num_actions = 0;
  bool error = false;
  for (int a = 0; a < NUM_ACTION_TYPES; ++a) {
    Action action;
    action.type = (ActionType)a;
    action.size = 0;
    if (action.type == a_raise) {
      int32_t min_raise_size;
      int32_t max_raise_size;
      int32_t pot_size = state.spent[0] + state.spent[1];
      if (raiseIsValid(game, &state, &min_raise_size, &max_raise_size)) {
          //std::cout << "min_raise: " << min_raise_size << "\tmax_size: " << max_raise_size << "\tmax_spent:" << state.maxSpent<< "" << std::endl;
      //std::cout << "min: " << min_raise_size << ", max: " << max_raise_size << ", pot: " << pot_size << ",max spent: " << state.maxSpent << "" << std::endl;
        for (int s = 0; s < fractions.size(); ++s) {
          int32_t raise_size = state.maxSpent + fractions[s] * pot_size;
          if( raise_size > max_raise_size )
              raise_size = max_raise_size;
          if( raise_size < min_raise_size )
              raise_size = min_raise_size;

          //std::cout << "raise idx: " << s << "\t size: " << raise_size << "" << std::endl;

          //std::cout << "attempting to raise: " << raise_size << "" << std::endl;

          assert(raise_size > 0);
          actions[num_actions] = action;
          actions[num_actions].size = raise_size;
          ++num_actions;

          if( raise_size >= max_raise_size )
              break;
        }
        // allin
          //actions[num_actions] = action;
          //actions[num_actions].size = 
          //++num_actions;
      }
    } else if (isValidAction(game, &state, 0, &action)) {
      /* If you hit this assert, there are too many abstract actions allowed.
       * Either coarsen the betting abstraction or increase
       * MAX_ABSTRACT_ACTIONS
       * in constants.hpp
       */
      if (num_actions >= MAX_ABSTRACT_ACTIONS) {
        error = true;
        break;
      }
      actions[num_actions] = action;
      ++num_actions;
    }
  }
  //std::cout << "actions: " << num_actions << "" << std::endl;

  actions.resize(num_actions);
  assert( !error );
  //for(unsigned i = 0; i < actions.size(); ++i){
      //std::cout << i  << "" << std::endl;
      //if(actions[i].type == a_raise)
        //assert(actions[i].size > 0);
  //}

  return actions;
}
