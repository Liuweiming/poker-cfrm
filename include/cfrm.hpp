#ifndef CFRM_HPP
#define CFRM_HPP

#include <algorithm>
#include <bitset>
#include <ecalc/macros.hpp>
#include <ecalc/types.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <poker/card.hpp>
#include <random>
#include <string>
#include "abstract_game.hpp"
#include "data_logger.h"

using std::vector;

class CFRM {
 public:
  AbstractGame *game;
  entry_c regrets;
  entry_c avg_strategy;
  int br_count;
  int num_threads;
  std::vector<double *> value_cache;
  bool *valid_cache;

  CFRM(AbstractGame *game, int num_threads=1)
      : game(game),
        num_threads(num_threads),
        value_cache(num_threads, nullptr),
        valid_cache(nullptr),
        regrets(game->get_nb_infosets()),
        avg_strategy(game->get_nb_infosets()) {
    game->game_tree_root()->init_entries(
        regrets, avg_strategy, game->get_gamedef(), game->card_abstraction());
  }

  CFRM(AbstractGame *game, char *strat_dump_file, int num_threads=1);

  virtual ~CFRM() {
    for (auto &p : value_cache) {
      if (p != nullptr) {
        delete[] p;
      }
    }
    if (valid_cache != nullptr) {
      delete[] valid_cache;
    }
  }

  INode *lookup_state(const State *state, int player);

  hand_t generate_hand(nbgen &rng);

  int draw_card(uint64_t &deckset, card_c deck, int deck_size, nbgen &rng);

  void print_strategy(unsigned player);

  DataLogger::Record debug();

  void print_strategy_r(unsigned player, INode *curr_node, std::string);

  std::vector<double> get_regret(uint64_t info_idx, int bucket);

  std::vector<double> get_strategy(uint64_t info_idx, int bucket);

  int sample_strategy(std::vector<double> strategy, nbgen &rng);

  vector<double> get_normalized_avg_strategy(uint64_t idx, int bucket);

  vector<double> get_normalized_avg_strategy(uint64_t idx, card_c hand,
                                             card_c board, int round);

  // calculate best response of the abstract game
  std::vector<double> abstract_best_response();
  vector<vector<double>> abstract_br(INode *curr_node,
                                     vector<vector<double>> op,
                                     std::string path);
  vector<vector<double>> abstract_br_infoset(INode *curr_node,
                                             vector<vector<double>> op,
                                             std::string path);

  vector<vector<double>> abstract_br_terminal(INode *curr_node,
                                              vector<vector<double>> op,
                                              std::string path);

  std::vector<vector<double>> br_public_chance(
      INode *curr_node, const card_c &deck, const hand_list &hands,
      vector<vector<double>> op, const vector<bool> &valid, std::string path);

  std::vector<vector<double>> br_private_chance(
      INode *curr_node, const card_c &deck, const hand_list &hands,
      vector<vector<double>> op, const vector<bool> &valid, std::string path);

  std::vector<vector<double>> br_terminal(INode *curr_node, const card_c &deck,
                                          const hand_list &hands,
                                          vector<vector<double>> op,
                                          const vector<bool> &valid,
                                          std::string path);

  std::vector<vector<double>> br_infoset(INode *curr_node, const card_c &deck,
                                         const hand_list &hands,
                                         vector<vector<double>> op,
                                         const vector<bool> &valid,
                                         std::string path);

  std::vector<vector<double>> best_response(
      INode *curr_node, const card_c &deck, const hand_list &hands,
      vector<vector<double>> op, const vector<bool> &valid, std::string path);

  std::vector<double> best_response();

  void dump(char *filename);

  // recursively counts the size of the subtree curr.
  size_t count_states(INode *curr) {
    if (curr->is_public_chance()) {
      PublicChanceNode *n = (PublicChanceNode *)curr;
      size_t sum = 0;
      for (unsigned i = 0; i < n->children.size(); ++i) {
        sum += count_states(n->children[i]);
      }
      return sum;
    } else if (curr->is_private_chance()) {
      return count_states(((PrivateChanceNode *)curr)->child);
    } else if (curr->is_terminal()) {
      if (curr->is_fold())
        return 0;
      else
        return 0;
    }
    InformationSetNode *n = (InformationSetNode *)curr;
    size_t sum = 0;

    uint64_t info_idx = n->get_idx();
    sum += avg_strategy[info_idx].nb_buckets;

    for (unsigned i = 0; i < n->children.size(); ++i) {
      sum += count_states(n->children[i]);
    }
    return sum;
  }

  // recursively counts the size of the subtree curr.
  size_t count_terminal_nodes(INode *curr) {
    if (curr->is_public_chance()) {
      PublicChanceNode *n = (PublicChanceNode *)curr;
      size_t sum = 0;
      for (unsigned i = 0; i < n->children.size(); ++i) {
        sum += count_terminal_nodes(n->children[i]);
      }
      return sum;
    } else if (curr->is_private_chance()) {
      return count_terminal_nodes(((PrivateChanceNode *)curr)->child);
    } else if (curr->is_terminal()) {
      if (curr->is_fold())
        return 1;
      else
        return 1;
    }
    InformationSetNode *n = (InformationSetNode *)curr;
    size_t sum = 0;
    for (unsigned i = 0; i < n->children.size(); ++i) {
      sum += count_terminal_nodes(n->children[i]);
    }
    return sum;
  }

  // recursively counts the size of the subtree curr.
  size_t count_bytes(INode *curr) {
    if (curr->is_public_chance()) {
      PublicChanceNode *n = (PublicChanceNode *)curr;
      size_t sum = 0;
      for (unsigned i = 0; i < n->children.size(); ++i) {
        sum += count_bytes(n->children[i]);
      }
      return sum;
    } else if (curr->is_private_chance()) {
      return count_bytes(((PrivateChanceNode *)curr)->child) +
             sizeof(PrivateChanceNode);
    } else if (curr->is_terminal()) {
      if (curr->is_fold())
        return sizeof(FoldNode);
      else
        return sizeof(ShowdownNode);
    }
    InformationSetNode *n = (InformationSetNode *)curr;
    size_t sum = 0;
    for (unsigned i = 0; i < n->children.size(); ++i) {
      sum += count_bytes(n->children[i]);
    }
    return sum;
  }

  virtual void iterate(nbgen &rng) = 0;
};

class ExternalSamplingCFR : public CFRM {
 public:
  ExternalSamplingCFR(AbstractGame *game, int num_threads=1) : CFRM(game, num_threads) {}
  ExternalSamplingCFR(AbstractGame *game, char *strat_dump_file, int num_threads=1)
      : CFRM(game, strat_dump_file, num_threads) {}

  virtual void iterate(nbgen &rng);

  double train(int trainplayer, hand_t hand, INode *curr_node, double p,
               double op, nbgen &rng);
};

class ChanceSamplingCFR : public CFRM {
 public:
  ChanceSamplingCFR(AbstractGame *game, int num_threads=1) : CFRM(game, num_threads) {}
  ChanceSamplingCFR(AbstractGame *game, char *strat_dump_file, int num_threads=1)
      : CFRM(game, strat_dump_file, num_threads) {}

  virtual void iterate(nbgen &rng);

  double train(int trainplayer, hand_t hand, INode *curr_node, double p,
               double op, nbgen &rng);
};

class OutcomeSamplingCFR : public CFRM {
 public:
  OutcomeSamplingCFR(AbstractGame *game, int num_threads=1) : CFRM(game, num_threads) {}
  OutcomeSamplingCFR(AbstractGame *game, char *strat_dump_file, int num_threads=1)
      : CFRM(game, strat_dump_file, num_threads) {}

  virtual void iterate(nbgen &rng);

  vector<double> train(hand_t hand, INode *curr_node, vector<double> reach,
                       double sp, nbgen &rng);
};

#endif
