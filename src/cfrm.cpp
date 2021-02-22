#include "cfrm.hpp"
#include <assert.h>
#include <omp.h>
#include <iostream>
#include <unordered_set>
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "data_logger.h"
#include "definitions.hpp"
#include "functions.hpp"

namespace {
card_c update_deck(card_c &deck, const card_c &board) {
  for (unsigned c = 0; c < board.size(); ++c) {
    for (unsigned di = 0; di != deck.size(); ++di) {
      if (deck[di] == board[c]) {
        deck[di] = deck.back();
        deck.pop_back();
        break;
      }
    }
  }
  std::sort(deck.begin(), deck.end());
  return deck;
}
}  // namespace

CFRM::CFRM(AbstractGame *game, char *strat_dump_file, int num_threads)
    : game(game),
      num_threads(num_threads),
      value_cache(num_threads, nullptr),
      valid_cache(nullptr) {
  std::ifstream file(strat_dump_file, std::ios::in | std::ios::binary);
  size_t nb_infosets = game->get_nb_infosets();
  file.read(reinterpret_cast<char *>(&nb_infosets), sizeof(nb_infosets));
  if (!file.is_open()) {
    std::cerr << "could not load strategy from" << strat_dump_file << std::endl;
    exit(-1);
  }
  assert(nb_infosets == game->get_nb_infosets());

  regrets = entry_c(nb_infosets);
  for (size_t i = 0; i < nb_infosets; ++i) {
    entry_t &entry = regrets[i];
    file.read(reinterpret_cast<char *>(&entry.nb_buckets),
              sizeof(entry.nb_buckets));
    file.read(reinterpret_cast<char *>(&entry.nb_entries),
              sizeof(entry.nb_entries));

    if (!file.good()) {
      std::cerr << "read regrets from" << strat_dump_file << "error"
                << std::endl;
    }
    entry.init(entry.nb_buckets, entry.nb_entries);
    std::vector<double> buff(entry.nb_buckets * entry.nb_entries);
    file.read(reinterpret_cast<char *>(&buff[0]),
              sizeof(buff[0]) * buff.size());
    if (!file.good()) {
      std::cerr << "read regrets from" << strat_dump_file << "error"
                << std::endl;
    }
    for (size_t j = 0; j != buff.size(); ++j) {
      entry[j] = buff[j];
    }
  }

  avg_strategy = entry_c(nb_infosets);
  for (size_t i = 0; i < nb_infosets; ++i) {
    entry_t &entry = avg_strategy[i];
    file.read(reinterpret_cast<char *>(&entry.nb_buckets),
              sizeof(entry.nb_buckets));
    file.read(reinterpret_cast<char *>(&entry.nb_entries),
              sizeof(entry.nb_entries));
    if (!file.good()) {
      std::cerr << "read strategy from" << strat_dump_file << "error"
                << std::endl;
    }
    entry.init(entry.nb_buckets, entry.nb_entries);
    std::vector<double> buff(entry.nb_buckets * entry.nb_entries);
    file.read(reinterpret_cast<char *>(&buff[0]),
              sizeof(buff[0]) * buff.size());
    if (!file.good()) {
      std::cerr << "read strategy from" << strat_dump_file << "error"
                << std::endl;
    }
    for (size_t j = 0; j != buff.size(); ++j) {
      entry[j] = buff[j];
    }
  }
}

std::vector<double> CFRM::abstract_best_response() {
  auto result =
      abstract_br(game->game_tree_root(),
                  vector<vector<double>>(game->get_gamedef()->numPlayers,
                                         vector<double>(1, 1.0)),
                  "");
  std::vector<double> out(result.size());
  for (unsigned i = 0; i < result.size(); ++i) out[i] = result[i][0];
  return out;
}

vector<std::vector<double>> CFRM::abstract_br(INode *curr_node,
                                              vector<vector<double>> op,
                                              std::string path) {
  if (curr_node->is_terminal()) {
    return abstract_br_terminal(curr_node, op, path);
  }
  return abstract_br_infoset(curr_node, op, path);
}

vector<vector<double>> CFRM::abstract_br_infoset(INode *curr_node,
                                                 vector<vector<double>> op,
                                                 std::string path) {
  InformationSetNode *node = (InformationSetNode *)curr_node;
  uint64_t info_idx = node->get_idx();
  int nb_buckets = game->card_abstraction()->get_nb_buckets(game->get_gamedef(),
                                                            node->get_round());

  vector<vector<double>> probabilities(nb_buckets);
  // std::cout << "path:" << path << "" << std::endl;;
  // std::cout << "info_idx:" << info_idx << "" << std::endl;;
  // std::cout << "idx:" << node->hand_idx << "" << std::endl;;
  // std::cout << "round:" << node->get_round() << "" << std::endl;;
  // std::cout << "path:" << path << "" << std::endl;;
  // std::cout << "prob:" << probabilities.size() << "" << std::endl;;
  // std::cout << "op:" << op.size() << "" << std::endl;;
  // std::cout << "buckets:" << nb_buckets << "" << std::endl;;
  for (unsigned i = 0; i < nb_buckets; ++i) {
    probabilities[i] = get_normalized_avg_strategy(info_idx, i);
  }

  // std::cout << "probabilities:" << std::endl;;
  // for (unsigned i = 0; i < probabilities.size(); ++i) {
  //   for (unsigned j = 0; j < probabilities[i].size(); ++j)
  //     std::cout << probabilities[i][j] << " ";
  //   std::cout << "" << std::endl;;
  // }

  vector<INode *> children = node->get_children();
  vector<vector<vector<double>>> payoffs(children.size());
  unsigned curr_player = node->get_player();
  for (unsigned a = 0; a < children.size(); ++a) {
    vector<vector<double>> newop(op);

    for (unsigned h = 0; h < newop[curr_player].size(); ++h) {
      newop[curr_player][h] *= probabilities[h][a];
    }
    payoffs[a] = abstract_br(children[a], newop, "");
  }

  unsigned opp = (curr_player + 1) % 2;
  double max_val = payoffs[0][curr_player][0];
  unsigned max_index = 0;
  for (unsigned a = 1; a < children.size(); ++a) {
    if (max_val < payoffs[a][curr_player][0]) {
      max_val = payoffs[a][curr_player][0];
      max_index = a;
    }
  }

  vector<vector<double>> result(2, vector<double>(1, 0));
  result[curr_player][0] = max_val;
  result[opp][0] = payoffs[max_index][opp][0];
  return result;
}

vector<vector<double>> CFRM::abstract_br_terminal(INode *curr_node,
                                                  vector<vector<double>> op,
                                                  std::string path) {
  vector<vector<double>> payoffs(op.size(), vector<double>(op[0].size(), 0));
  vector<vector<double>> result(op.size(), vector<double>(1, 0));

  if (curr_node->is_fold()) {
    FoldNode *node = (FoldNode *)curr_node;
    int fold_player = node->fold_player;
    int money_f = node->value;

    vector<double> opp_ges_p(2, 0);
    for (unsigned p = 0; p < op.size(); ++p) {
      unsigned opp = (p + 1) % 2;
      for (unsigned g = 0; g < op[0].size(); ++g) {
        payoffs[p][g] = op[opp][g] * (p == fold_player ? -1.0 : 1.0) * money_f;
        result[p][0] += payoffs[p][g];
        opp_ges_p[p] += op[opp][g];
      }
      result[p][0] *= (1.0 / opp_ges_p[p]);
    }

    return result;
  }

  // showdown
  ShowdownNode *node = (ShowdownNode *)curr_node;

  int money = node->value;

  vector<double> opp_ges_p(2, 0);
  for (unsigned p = 0; p < op.size(); ++p) {
    unsigned opp = (p + 1) % 2;
    for (unsigned g = 0; g < op[0].size(); ++g) {
      payoffs[p][g] = op[opp][g] * money;
      result[p][0] += payoffs[p][g];
      opp_ges_p[p] += op[opp][g];
    }
    result[p][0] *= (1.0 / opp_ges_p[p]);
  }

  return result;
}

INode *CFRM::lookup_state(const State *state, int player) {
  return game->lookup_state(state, player, game->game_tree_root(), 0, 0);
}

hand_t CFRM::generate_hand(nbgen &rng) {
  card_c deck = game->generate_deck(game->get_gamedef()->numRanks,
                                    game->get_gamedef()->numSuits);

  uint64_t deckset = -1;
  int deck_size = game->deck_size();
  std::vector<card_c> hand(game->nb_players(), card_c(game->hand_size()));
  for (int p = 0; p < game->nb_players(); ++p) {
    for (int c = 0; c < game->hand_size(); ++c) {
      hand[p][c] = draw_card(deckset, deck, deck.size(), rng);
    }
  }

  card_c board;
  for (int r = 0; r < game->nb_rounds(); ++r) {
    for (int i = 0; i < game->nb_boardcards(r); ++i) {
      board.push_back(draw_card(deckset, deck, deck.size(), rng));
    }
  }

  hand_t handc(hand, board);
  game->evaluate(handc);
  return handc;
}

int CFRM::draw_card(uint64_t &deckset, card_c deck, int deck_size, nbgen &rng) {
  using ecalc::bitset;
  int rand;
  while (true) {
    rand = rng() % deck_size;
    if (BIT_GET(deckset, rand)) {
      deckset = BIT_CLR(deckset, rand);
      return deck[rand];
    }
  }
}

void CFRM::print_strategy(unsigned player) {
  print_strategy_r(player, game->game_tree_root(), "");
}

void CFRM::print_strategy_r(unsigned player, INode *curr_node,
                            std::string path) {
  if (curr_node->is_terminal()) return;

  InformationSetNode *node = (InformationSetNode *)curr_node;
  uint64_t info_idx = node->get_idx();
  int nb_buckets = game->card_abstraction()->get_nb_buckets(game->get_gamedef(),
                                                            node->get_round());
  vector<INode *> children = node->get_children();
  int round = node->get_round();

  Action last_action;
  std::string newpath;

  if (node->get_player() == player) {
    std::cout << path << ":" << std::endl;
    ;
    for (unsigned i = 0; i < children.size(); ++i) {
      last_action = children[i]->get_action();
      std::cout << " " << ActionsStr[last_action.type]
                << (last_action.size > 0 ? std::to_string(last_action.size)
                                         : "")
                << " " << std::fixed;
      for (unsigned b = 0; b < nb_buckets; ++b) {
        auto strategy = get_normalized_avg_strategy(info_idx, b);
        std::cout << std::setprecision(3) << strategy[i] << " ";
      }
      std::cout << "" << std::endl;
    }
    std::cout << "" << std::endl;

    for (unsigned i = 0; i < children.size(); ++i) {
      std::string phase_sw =
          (round != ((InformationSetNode *)children[i])->get_round()) ? "/"
                                                                      : "";
      last_action = children[i]->get_action();
      print_strategy_r(
          player, children[i],
          path + ActionsStr[last_action.type] +
              (last_action.size > 0 ? std::to_string(last_action.size) : "") +
              phase_sw);
    }
  } else {
    for (unsigned i = 0; i < children.size(); ++i) {
      std::string phase_sw =
          (round != ((InformationSetNode *)children[i])->get_round()) ? "/"
                                                                      : "";
      last_action = children[i]->get_action();
      print_strategy_r(
          player, children[i],
          path + ActionsStr[last_action.type] +
              (last_action.size > 0 ? std::to_string(last_action.size) : "") +
              phase_sw);
    }
  }
}

std::vector<double> CFRM::get_strategy(uint64_t info_idx, int bucket) {
  entry_t &reg = regrets[info_idx];
  unsigned nb_children = reg.nb_entries;
  std::vector<double> strategy(nb_children);
  double psum = 0;

  for (unsigned i = 0; i < nb_children; ++i)
    if (reg[bucket * reg.nb_entries + i] > 0) {
      psum += reg[bucket * reg.nb_entries + i];
    }

  if (psum > 0) {
    for (unsigned i = 0; i < nb_children; ++i) {
      strategy[i] = (reg[bucket * reg.nb_entries + i] > 0)
                        ? (reg[bucket * reg.nb_entries + i] / psum)
                        : 0.0;
    }
  } else {
    for (unsigned i = 0; i < nb_children; ++i) {
      strategy[i] = 1.0 / nb_children;
    }
  }

  return strategy;
}

std::vector<double> CFRM::get_regret(uint64_t info_idx, int bucket) {
  entry_t &reg = regrets[info_idx];
  unsigned nb_children = reg.nb_entries;
  std::vector<double> regret(nb_children);

  for (unsigned i = 0; i < nb_children; ++i) {
    regret[i] = reg[bucket * reg.nb_entries + i];
  }

  return regret;
}

int CFRM::sample_strategy(std::vector<double> strategy, nbgen &rng) {
  std::discrete_distribution<int> d(strategy.begin(), strategy.end());
  return d(rng);
}

vector<double> CFRM::get_normalized_avg_strategy(uint64_t idx, int bucket) {
  entry_t &avg = avg_strategy[idx];
  unsigned nb_choices = avg.nb_entries;
  vector<double> strategy(nb_choices);
  double sum = 0;

  for (unsigned i = 0; i < nb_choices; ++i) {
    double v = avg[bucket * avg.nb_entries + i];
    sum += (v < 0) ? 0 : v;
  }

  if (sum > 0) {
    for (unsigned i = 0; i < nb_choices; ++i) {
      double v = avg[bucket * avg.nb_entries + i];
      strategy[i] = (v > 0) ? (v / sum) : 0;
    }
  } else {
    for (unsigned i = 0; i < nb_choices; ++i) {
      strategy[i] = 1.0 / nb_choices;
    }
  }

  return strategy;
}

vector<double> CFRM::get_normalized_avg_strategy(uint64_t idx, card_c hand,
                                                 card_c board, int round) {
  int bucket = game->card_abstraction()->map_hand_to_bucket(hand, board, round);
  auto st = get_normalized_avg_strategy(idx, bucket);
  // std::cout << "bucket: " << bucket << " strategy: [";
  // for (auto s : st) {
  //   std::cout << s << " ";
  // }
  // std::cout << "]" << std::endl;
  return st;
}

std::vector<vector<double>> CFRM::br_public_chance(
    INode *curr_node, const card_c &deck, const hand_list &hands,
    vector<vector<double>> op, const vector<bool> &valid, std::string path) {
  const Game *def = game->get_gamedef();
  PublicChanceNode *p = (PublicChanceNode *)curr_node;
  int nb_dead = p->board.size() + def->numHoleCards;
  unsigned possible_deals =
      choose((def->numRanks * def->numSuits) - nb_dead, p->to_deal);

  vector<vector<double>> payoffs(op.size(), vector<double>(op[0].size(), 0));
  br_count += 1;
  std::cout << br_count << "/7" << std::endl;
#pragma omp parallel for num_threads(num_threads)
  for (unsigned child = 0; child < p->children.size(); ++child) {
    InformationSetNode *n = (InformationSetNode *)p->children[child];
    card_c new_deck = game->generate_deck(def->numRanks, def->numSuits);
    new_deck = update_deck(new_deck, n->board);
    int thread_idx = omp_get_thread_num();
    if (thread_idx > num_threads) {
      std::cerr << "omp thread idx larger than num_threads" << std::endl;
    }
    if (value_cache[thread_idx] != nullptr) {
      delete[] value_cache[thread_idx];
    }
    value_cache[thread_idx] = nullptr;

    std::string newpath = path;
    newpath = path + ActionsStr[n->get_action().type] + "/";

    vector<vector<double>> newop = op;
    vector<bool> new_valid = valid;
    for (unsigned i = 0; i < op.size(); ++i) {
      for (unsigned j = 0; j < newop[i].size(); ++j) {
        if (game->do_intersect(hands[j], n->board)) {
          newop[i][j] = 0;
          new_valid[j] = 0;
        } else {
          newop[i][j] = op[i][j] / possible_deals;
        }
      }
    }
    auto subpayoffs = best_response(p->children[child], new_deck, hands, newop,
                                    new_valid, newpath);
    for (unsigned i = 0; i < subpayoffs.size(); ++i) {
      for (unsigned j = 0; j < subpayoffs[i].size(); ++j) {
#pragma omp critical
        payoffs[i][j] += subpayoffs[i][j];
      }
    }
  }
  return payoffs;
}

std::vector<vector<double>> CFRM::br_private_chance(
    INode *curr_node, const card_c &deck, const hand_list &hands,
    vector<vector<double>> op, const vector<bool> &valid, std::string path) {
  const Game *def = game->get_gamedef();
  PrivateChanceNode *p = (PrivateChanceNode *)curr_node;
  unsigned possible_deals = choose(def->numRanks * def->numSuits, p->to_deal);
  vector<vector<double>> newop(op.size());
  for (unsigned i = 0; i < op.size(); ++i) {
    newop[i] = vector<double>(possible_deals, op[i][0] / possible_deals);
  }
  vector<bool> new_valid(possible_deals, true);

  vector<vector<double>> subpayoffs =
      best_response(p->child, deck, hands, newop, new_valid, path);
  vector<vector<double>> payoffs(op.size(), vector<double>(op[0].size(), 0));
  for (unsigned i = 0; i < subpayoffs.size(); ++i) {
    for (unsigned j = 0; j < possible_deals; ++j) {
      payoffs[i][0] += subpayoffs[i][j];
    }
  }

  return payoffs;
}

std::vector<vector<double>> CFRM::br_terminal(
    INode *curr_node, const card_c &deck, const hand_list &hands,
    vector<vector<double>> op, const vector<bool> &valid, std::string path) {
  unsigned player = 0;
  unsigned opponent = (player + 1) % 2;
  vector<vector<double>> payoffs(op.size(), vector<double>(op[0].size(), 0));
  if (valid_cache == nullptr) {
    valid_cache = new bool[hands.size() * hands.size()];
    for (unsigned i = 0; i < hands.size(); ++i) {
      for (unsigned j = 0; j < hands.size(); ++j) {
        if (i == j || game->do_intersect(hands[i], hands[j]))
          valid_cache[i * hands.size() + j] = false;
        else
          valid_cache[i * hands.size() + j] = true;
      }
    }
  }

  if (curr_node->is_fold()) {
    FoldNode *node = (FoldNode *)curr_node;
    int fold_player = node->fold_player;
    double money_f = node->value;
    
    const Game *def = game->get_gamedef();
    int nb_dead = node->board.size() + def->numHoleCards;
    unsigned possible_deals =
        choose((def->numRanks * def->numSuits) - nb_dead, def->numHoleCards);

    for (unsigned i = 0; i < hands.size(); ++i) {
      if (!valid[i]) {
        continue;
      }
      for (unsigned j = 0; j < hands.size(); ++j) {
        if (!valid[j] || !valid_cache[i * hands.size() + j]) {
          continue;
        }
        double payoff =
            (fold_player == 0 ? -1.0 : 1.0) * money_f / possible_deals;
        payoffs[0][i] += op[1][j] * payoff;

        payoffs[1][j] += op[0][i] * (-payoff);
      }
    }
    return payoffs;
  } else {
    // showdown
    ShowdownNode *node = (ShowdownNode *)curr_node;
    double money = node->value;

    int thread_idx = omp_get_thread_num();
    if (value_cache[thread_idx] == nullptr) {
      const Game *def = game->get_gamedef();
      int nb_dead = node->board.size() + def->numHoleCards;
      unsigned possible_deals =
          choose((def->numRanks * def->numSuits) - nb_dead, def->numHoleCards);
      value_cache[thread_idx] = new double[hands.size() * hands.size()];
      vector<int> ranks(hands.size());
      for (unsigned i = 0; i < hands.size(); ++i) {
        if (valid[i]) {
          ranks[i] = game->rank_hand(hands[i], node->board);
        } else {
          ranks[i] = -1;
        }
      }

      for (unsigned i = 0; i < hands.size(); ++i) {
        for (unsigned j = 0; j < hands.size(); ++j) {
          if (!valid[i] || !valid[j] || !valid_cache[i * hands.size() + j]) {
            value_cache[thread_idx][i * hands.size() + j] = 0;
          } else {
            double comp = 0;
            if (ranks[i] > ranks[j]) {
              comp = 1;
            } else if (ranks[i] < ranks[j]) {
              comp = -1;
            }
            value_cache[thread_idx][i * hands.size() + j] =
                comp / possible_deals;
          }
        }
      }
    }
    for (unsigned i = 0; i < hands.size(); ++i) {
      if (!valid[i]) {
        continue;
      }
      for (unsigned j = 0; j < hands.size(); ++j) {
        if (!valid[j] || !valid_cache[i * hands.size() + j]) {
          continue;
        }
        payoffs[0][i] +=
            op[1][j] * value_cache[thread_idx][i * hands.size() + j] * money;

        payoffs[1][j] +=
            op[0][i] * (-value_cache[thread_idx][i * hands.size() + j]) * money;
      }
    }
    return payoffs;
  }
}

std::vector<vector<double>> CFRM::br_infoset(
    INode *curr_node, const card_c &deck, const hand_list &hands,
    vector<vector<double>> op, const vector<bool> &valid, std::string path) {
  InformationSetNode *node = (InformationSetNode *)curr_node;
  const Game *def = game->get_gamedef();
  uint64_t info_idx = node->get_idx();
  int nb_buckets = game->card_abstraction()->get_nb_buckets(game->get_gamedef(),
                                                            node->get_round());

  vector<vector<double>> probabilities(hands.size());

  for (unsigned i = 0; i < op[0].size(); ++i) {
    auto hand = hands[i];
    if (valid[i]) {
      probabilities[i] = get_normalized_avg_strategy(
          info_idx, hand, node->board, node->get_round());
    } else {
      probabilities[i] = vector<double>(node->get_children().size(), 0.0);
    }
  }

  vector<vector<vector<double>>> action_payoffs(node->get_children().size());
  for (unsigned i = 0; i < node->get_children().size(); ++i) {
    vector<vector<double>> newop(op);
    for (unsigned j = 0; j < probabilities.size(); ++j) {
      newop[node->get_player()][j] *= probabilities[j][i];
    }
    std::string newpath = path;
    if (!node->get_children()[i]->is_chance())
      newpath = path + ActionsStr[node->get_children()[i]->get_action().type];
    action_payoffs[i] = best_response(node->get_children()[i], deck, hands,
                                      newop, valid, newpath);
  }

  vector<vector<double>> payoffs(op.size());
  for (unsigned i = 0; i < op.size(); ++i) {
    if (i == node->get_player()) {
      payoffs[i] = vector<double>(op[i].size(), 0);
      for (unsigned j = 0; j < op[i].size(); ++j) {
        double max_value = DOUBLE_MAX * -1;
        for (unsigned action = 0; action < action_payoffs.size(); ++action) {
          double value = action_payoffs[action][i][j];
          if (value > max_value) {
            max_value = value;
          }
        }
        payoffs[i][j] = max_value;
      }
    } else {
      payoffs[i] = vector<double>(op[i].size(), 0);
      for (unsigned j = 0; j < op[i].size(); ++j) {
        for (unsigned action = 0; action < action_payoffs.size(); ++action) {
          if (action_payoffs[action][i].size() == 0) continue;
          payoffs[i][j] += action_payoffs[action][i][j];
        }
      }
    }
  }

  return payoffs;
}

std::vector<vector<double>> CFRM::best_response(
    INode *curr_node, const card_c &deck, const hand_list &hands,
    vector<vector<double>> op, const vector<bool> &valid, std::string path) {
  if (curr_node->is_public_chance()) {
    return br_public_chance(curr_node, deck, hands, op, valid, path);
  } else if (curr_node->is_private_chance()) {
    return br_private_chance(curr_node, deck, hands, op, valid, path);
  } else if (curr_node->is_terminal()) {
    return br_terminal(curr_node, deck, hands, op, valid, path);
  }
  return br_infoset(curr_node, deck, hands, op, valid, path);
}

std::vector<double> CFRM::best_response() {
  const Game *def = game->get_gamedef();
  card_c deck = game->generate_deck(def->numRanks, def->numSuits);
  hand_list hands = deck_to_combinations(def->numHoleCards, deck);
  br_count = 0;
  auto result =
      best_response(game->public_tree_root(), deck, hands,
                    vector<vector<double>>(game->get_gamedef()->numPlayers,
                                           vector<double>(1, 1.0)),
                    vector<bool>(1, true), "");
  std::vector<double> out(result.size());
  for (unsigned i = 0; i < result.size(); ++i) out[i] = result[i][0];
  return out;
}

DataLogger::Record CFRM::debug() {
  // std::unordered_set<std::string> infos = {
  //     "STATE:0:crc/r:Qh|/Qs",   "STATE:0:crrc/r:As|/Qh",
  //     "STATE:0:rc/r:Kh|/Ah",    "STATE:0:crc/cr:Qh|/Qs",
  //     "STATE:0:crc/c:Kh|/Qh",   "STATE:0:rrc/cr:Qs|/As",
  //     "STATE:0:cc/cr:Ks|/Qh",   "STATE:0:crrc/:As|/Qs",
  //     "STATE:0:crrc/cr:Qh|/Kh", "STATE:0:crr:Qh|/",
  //     "STATE:0:rc/crr:Qs|/Ks",  "STATE:0:crrc/rr:Ah|/Kh",
  //     "STATE:0:crc/crr:Ks|/As", "STATE:0:rrc/crr:Ks|/Qh",
  //     "STATE:0:cc/rr:Kh|/Ks",   "STATE:0:cc/r:Qs|/Kh",
  //     "STATE:0::As|/",          "STATE:0:crc/rr:Ks|/As",
  //     "STATE:0:cc/crr:Kh|/Qs",  "STATE:0:rc/cr:As|/Qs",
  //     "STATE:0:r:Ks|/",         "STATE:0:crrc/c:As|/Ah",
  //     "STATE:0:rr:Qh|/",        "STATE:0:rrc/r:Ah|/Ks",
  // };
  std::unordered_set<std::string> infos = {
      "STATE:0:crc/crr:KsAh|/ThJhJs",
      "STATE:0:crrc/cr:QsKh|/TsJsAs",
      "STATE:0:cr:ThKs|/",
      "STATE:0:rc/:ThAs|/TsJsQs",
      "STATE:0:rrc/rr:TsAs|/ThJhKh",
      "STATE:0:cc/r:ThKh|/JsQsAh",
      "STATE:0:crrc/rr:JhJs|/ThKsAh",
      "STATE:0:rrc/r:TsAh|/QhQsKs",
      "STATE:0:crrr:QhKh|/",
      "STATE:0:rc/rrr:QsAh|/JsQhKs",
      "STATE:0:cc/rr:KsAh|/ThKhAs",
      "STATE:0:crr/:QhKh|/",
      "STATE:0:rrrc/rrr:JhQh|/ThTsAs",
      "STATE:0:rrr:QsAh|/",
      "STATE:0:rc/r:QhKs|/ThQsAs",
      "STATE:0:crrrc/c:TsAh|/JhJsKs",
      "STATE:0:crc/rrr:JhAh|/QsKhAs",
      "STATE:0:rc/crr:JsKh|/QhAhAs",
      "STATE:0:c:QhAs|/",
      "STATE:0:rrrc/r:TsJh|/QhQsAs",
      "STATE:0:crrrc/crr:TsQs|/ThJsKh",
      "STATE:0:cc/rrr:AhAs|/JhQhKh",
      "STATE:0:crrrc/rr:QsAh|/JsQhAs",
      "STATE:0:crrc/rrr:JhKh|/ThJsQs",
      "STATE:0:rrrc/c:TsKs|/JhJsQh",
      "STATE:0:rrrc/cr:ThKs|/TsQhAh",
      "STATE:0:crrrc/:ThQs|/QhKhKs",
      "STATE:0:rrc/crrr:TsQs|/ThJhAh",
      "STATE:0:crrc/:AhAs|/TsJsKh",
      "STATE:0:rrrc/:JsAh|/TsJhKs",
      "STATE:0:rc/crrr:AhAs|/JhQhQs",
      "STATE:0:cc/:AhAs|/QhKhKs",
      "STATE:0::KsAh|",
      "STATE:0:crc/r:TsKs|/JsKhAh",
      "STATE:0:cc/c:QhQs|/TsJhJs",
      "STATE:0:crrc/c:QhAs|/ThJhKs",
      "STATE:0:crc/crrr:QhAs|/JsQsAh",
      "STATE:0:r:ThQs|/",
      "STATE:0:rc/cr:ThKs|/JhQhKh",
  };
  MatchState state;
  state.viewingPlayer = 0;
  const Game *gamedef = game->get_gamedef();
  DataLogger::Record info_record;
  for (auto &info : infos) {
    // std::cout << info << std::endl;
    readState(info.c_str(), gamedef, &state.state);
    InformationSetNode *curr_node =
        (InformationSetNode *)lookup_state(&state.state, state.viewingPlayer);
    // CHECK IF WE FOUND THE CORRECT NODE
    if (curr_node != NULL) {
      // std::cout << curr_node->get_info_str() << " " << info << std::endl;
      // std::cout << "lookup was successful";
      // get strategy at curr_node
      card_c hand(gamedef->numHoleCards);
      for (int i = 0; i < gamedef->numHoleCards; ++i) {
        int r = rankOfCard(state.state.holeCards[state.viewingPlayer][i]);
        int s = suitOfCard(state.state.holeCards[state.viewingPlayer][i]);
        hand[i] = makeCard(-MAX_RANKS + gamedef->numRanks + r,
                           -MAX_SUITS + gamedef->numSuits + s);
      }

      card_c board;
      for (int c = 0; c < sumBoardCards(gamedef, state.state.round); ++c) {
        int r = rankOfCard(state.state.boardCards[c]);
        int s = suitOfCard(state.state.boardCards[c]);
        board.push_back(makeCard(-MAX_RANKS + gamedef->numRanks + r,
                                 -MAX_SUITS + gamedef->numSuits + s));
      }
      char card_str[100];
      if (hand.size()) {
        printCards(hand.size(), &(hand[0]), 100, card_str);
        // std::cout << card_str << ":";
      }
      if (board.size()) {
        printCards(board.size(), &(board[0]), 100, card_str);
        // std::cout << card_str << std::endl;
      }

      int bucket = game->card_abstraction()->map_hand_to_bucket(
          hand, board, state.state.round);
      auto strategy = get_normalized_avg_strategy(curr_node->get_idx(), bucket);
      auto regret = get_regret(curr_node->get_idx(), bucket);
      // for (unsigned i = 0; i < regret.size(); ++i){
      //   std::cout << regret[i] << " ";
      // }
      // std::cout << std::endl;
      // for (unsigned i = 0; i < strategy.size(); ++i){
      //   std::cout << strategy[i] << " ";
      // }
      // std::cout << std::endl;
      std::vector<Action> actions = curr_node->get_actions();
      std::vector<std::string> str_actions(actions.size());
      for (int ai = 0; ai != actions.size(); ++ai) {
        static const std::string action_strs[3] = {"f", "c", "r"};
        str_actions[ai] = action_strs[actions[ai].type];
      }
      info_record.emplace(info, json::Object({
                                    {"action", json::CastToArray(str_actions)},
                                    {"regret_net", json::CastToArray(regret)},
                                    {"policy_net", json::CastToArray(strategy)},
                                }));
    }
  }
  return info_record;
}

void CFRM::dump(char *filename) {
  std::ofstream fs(filename, std::ios::out | std::ios::binary);
  size_t nb_infosets = regrets.size();
  fs.write(reinterpret_cast<const char *>(&nb_infosets), sizeof(nb_infosets));
  for (unsigned i = 0; i < nb_infosets; ++i) {
    fs.write(reinterpret_cast<const char *>(&regrets[i].nb_buckets),
             sizeof(regrets[i].nb_buckets));
    fs.write(reinterpret_cast<const char *>(&regrets[i].nb_entries),
             sizeof(regrets[i].nb_entries));
    std::vector<double> buff(regrets[i].size());
    for (size_t j = 0; j != buff.size(); ++j) {
      buff[j] = regrets[i][j];
    }
    fs.write(reinterpret_cast<const char *>(&buff[0]),
             sizeof(buff[0]) * (buff.size()));
  }
  for (unsigned i = 0; i < nb_infosets; ++i) {
    fs.write(reinterpret_cast<const char *>(&avg_strategy[i].nb_buckets),
             sizeof(avg_strategy[i].nb_buckets));
    fs.write(reinterpret_cast<const char *>(&avg_strategy[i].nb_entries),
             sizeof(avg_strategy[i].nb_entries));
    std::vector<double> buff(avg_strategy[i].size());
    for (size_t j = 0; j != buff.size(); ++j) {
      buff[j] = avg_strategy[i][j];
    }
    fs.write(reinterpret_cast<const char *>(&buff[0]),
             sizeof(buff[0]) * (buff.size()));
  }
  fs.close();
}

void ExternalSamplingCFR::iterate(nbgen &rng) {
  hand_t hand = generate_hand(rng);
  train(0, hand, game->game_tree_root(), 1, 1, rng);
  train(1, hand, game->game_tree_root(), 1, 1, rng);
}

double ExternalSamplingCFR::train(int trainplayer, hand_t hand,
                                  INode *curr_node, double p, double op,
                                  nbgen &rng) {
  if (curr_node->is_terminal()) {
    if (curr_node->is_fold()) {
      FoldNode *node = (FoldNode *)curr_node;
      if (trainplayer == node->get_player()) return -node->value;
      return node->value;
    }
    // else showdown
    return hand.value[trainplayer] * ((ShowdownNode *)curr_node)->value;
  } else {
    InformationSetNode *node = (InformationSetNode *)curr_node;
    auto info_str = node->get_info_str();
    std::vector<std::string> info_strs = absl::StrSplit(info_str, ":");
    uint64_t info_idx = node->get_idx();
    int bucket = game->card_abstraction()->map_hand_to_bucket(
        hand.holes[node->get_player()], hand.board, node->get_round());
#ifdef DEBUG
    char hole_str[100];
    char hole_str_1[100];
    char board_str[100];
    if (hand.holes[0].size()) {
      printCards(hand.holes[0].size(), &(hand.holes[0][0]), 100, hole_str);
    }
    if (hand.holes[1].size()) {
      printCards(hand.holes[1].size(), &(hand.holes[1][0]), 100, hole_str_1);
    }
    if (hand.board.size()) {
      printCards(hand.board.size(), &(hand.board[0]), 100, board_str);
    }
    if ((info_strs[2] == "" || info_strs[2] == "c" || info_strs[2] == "cc/" ||
         info_strs[2] == "cc/r") &&
        std::string(hole_str) == "2c5c" && std::string(board_str) == "3d4d6c") {
      std::cout << info_strs[2] << ":" << hole_str << "|" << hole_str_1 << "/"
                << board_str << " value " << (int)hand.value[0] << " "
                << (int)hand.value[1]
                << (trainplayer == node->get_player() ? std::string(" reg ")
                                                      : std::string(" st "))
                << bucket << std::endl;
      auto regret = get_regret(node->get_idx(), bucket);
      std::cout << "reg " << regret << std::endl;
      auto strategy = get_strategy(node->get_idx(), bucket);
      std::cout << "st " << strategy << std::endl;
      auto ave_strategy = get_normalized_avg_strategy(node->get_idx(), bucket);
      std::cout << "ave " << ave_strategy << std::endl;
      std::cout << std::endl;
    }
#endif
    if (node->get_player() == trainplayer) {
      auto strategy = get_strategy(node->get_idx(), bucket);

      std::vector<double> utils(strategy.size());
      double ev = 0;

      for (unsigned i = 0; i < strategy.size(); ++i) {
        utils[i] = train(trainplayer, hand, node->get_children()[i],
                         p * strategy[i], op, rng);
        ev += utils[i] * strategy[i];
      }

      entry_t &reg = regrets[info_idx];
      for (unsigned i = 0; i < strategy.size(); ++i) {
        regrets[info_idx][bucket * reg.nb_entries + i] += utils[i] - ev;
      }

      return ev;
    } else {
      auto strategy = get_strategy(info_idx, bucket);
      int action = sample_strategy(strategy, rng);
      entry_t &avg = avg_strategy[info_idx];
      for (unsigned i = 0; i < strategy.size(); ++i)
        avg_strategy[info_idx][bucket * avg.nb_entries + i] += strategy[i];
      return train(trainplayer, hand, node->get_children()[action], p, op, rng);
    }
  }
}

void ChanceSamplingCFR::iterate(nbgen &rng) {
  hand_t hand = generate_hand(rng);
  train(0, hand, game->game_tree_root(), 1, 1, rng);
  train(1, hand, game->game_tree_root(), 1, 1, rng);
}

double ChanceSamplingCFR::train(int trainplayer, hand_t hand, INode *curr_node,
                                double p, double op, nbgen &rng) {
  if (curr_node->is_terminal()) {
    if (curr_node->is_fold()) {
      FoldNode *node = (FoldNode *)curr_node;
      if (trainplayer == node->get_player()) return op * -node->value;
      return op * node->value;
    }
    // else showdown
    return hand.value[trainplayer] * ((ShowdownNode *)curr_node)->value * op;
  } else {
    InformationSetNode *node = (InformationSetNode *)curr_node;
    uint64_t info_idx = node->get_idx();
    int bucket = game->card_abstraction()->map_hand_to_bucket(
        hand.holes[node->get_player()], hand.board, node->get_round());

    if (node->get_player() == trainplayer) {
      auto strategy = get_strategy(node->get_idx(), bucket);

      entry_t &avg = avg_strategy[info_idx];
      for (unsigned i = 0; i < strategy.size(); ++i)
        avg_strategy[info_idx][bucket * avg.nb_entries + i] += p * strategy[i];

      std::vector<double> utils(strategy.size());
      double ev = 0;

      for (unsigned i = 0; i < strategy.size(); ++i) {
        utils[i] = train(trainplayer, hand, node->get_children()[i],
                         p * strategy[i], op, rng);
        ev += utils[i] * strategy[i];
      }

      entry_t &reg = regrets[info_idx];
      for (unsigned i = 0; i < strategy.size(); ++i) {
        regrets[info_idx][bucket * reg.nb_entries + i] += utils[i] - ev;
      }

      return ev;
    } else {
      auto strategy = get_strategy(info_idx, bucket);
      double ev = 0;
      for (unsigned i = 0; i < strategy.size(); ++i) {
        ev += train(trainplayer, hand, node->get_children()[i], p,
                    op * strategy[i], rng);
      }

      return ev;
    }
  }
}

void OutcomeSamplingCFR::iterate(nbgen &rng) {
  hand_t hand = generate_hand(rng);
  train(hand, game->game_tree_root(), vector<double>(2, 1), 1, rng);
}

vector<double> OutcomeSamplingCFR::train(hand_t hand, INode *curr_node,
                                         vector<double> reach, double sp,
                                         nbgen &rng) {
  if (curr_node->is_terminal()) {
    if (curr_node->is_fold()) {
      FoldNode *node = (FoldNode *)curr_node;
      int foldp = node->get_player();
      std::vector<double> values{foldp == 0 ? -node->value : node->value,
                                 foldp == 1 ? -node->value : node->value};
      return vector<double>{reach[1] * values[0] / sp,
                            reach[0] * values[1] / sp};
    }
    // else showdown
    double sdv = hand.value[0];
    return vector<double>{
        sdv * reach[1] * ((ShowdownNode *)curr_node)->value / sp,
        -sdv * reach[0] * ((ShowdownNode *)curr_node)->value / sp};
  } else {
    InformationSetNode *node = (InformationSetNode *)curr_node;
    uint64_t info_idx = node->get_idx();
    int bucket = game->card_abstraction()->map_hand_to_bucket(
        hand.holes[node->get_player()], hand.board, node->get_round());

    auto strategy = get_strategy(node->get_idx(), bucket);

    entry_t &avg = avg_strategy[info_idx];
    for (unsigned i = 0; i < strategy.size(); ++i)
      avg_strategy[info_idx][bucket * avg.nb_entries + i] +=
          (reach[node->get_player()] * strategy[i]) / sp;

    const double exploration = 0.6;
    int sampled_action;

    std::discrete_distribution<int> d{exploration, 1 - exploration};
    if (d(rng) == 0) {
      sampled_action = rng() % strategy.size();
    } else {
      sampled_action = sample_strategy(strategy, rng);
    }

    reach[node->get_player()] *= strategy[sampled_action];
    double csp = exploration * (1.0 / strategy.size()) +
                 (1 - exploration) * strategy[sampled_action];
    auto ev =
        train(hand, node->get_children()[sampled_action], reach, sp * csp, rng);

    entry_t &reg = regrets[info_idx];
    regrets[info_idx][bucket * reg.nb_entries + sampled_action] +=
        ev[node->get_player()];
    ev[node->get_player()] *= strategy[sampled_action];

    for (unsigned i = 0; i < strategy.size(); ++i) {
      regrets[info_idx][bucket * reg.nb_entries + i] += -ev[node->get_player()];
    }

    return ev;
  }
}
