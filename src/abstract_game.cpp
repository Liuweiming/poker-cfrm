#include "abstract_game.hpp"
#include <algorithm>
#include <ecalc/ecalc.hpp>
#include <ecalc/types.hpp>
#include "evalHandTables"
#include "functions.hpp"
#include "game.h"

AbstractGame::AbstractGame(const Game *game_definition,
                           CardAbstraction *card_abs,
                           ActionAbstraction *action_abs, int nb_threads)
    : game(game_definition),
      card_abs(card_abs),
      action_abs(action_abs),
      nb_threads(nb_threads) {
  uint64_t idx = 0, idi = 0;
  State initial_state;
  initState(game, 0, &initial_state);

  game_tree = init_game_tree({a_invalid, 0}, initial_state, game, idx);
  public_tree = NULL;

  nb_infosets = idx;
}

AbstractGame::~AbstractGame() {}

INode *AbstractGame::init_game_tree(Action action, State &state,
                                    const Game *game, uint64_t &idx) {
  if (state.finished) {
    if (!(state.playerFolded[0] || state.playerFolded[1])) {
      int money = state.spent[0];
      return new ShowdownNode(action, money);
    } else {
      int fold_player = state.playerFolded[0] ? 0 : 1;
      int money = state.spent[fold_player];
      return new FoldNode(action, fold_player, money);
    }
  }

  std::vector<Action> actions = action_abs->get_actions(game, state);

  std::vector<INode *> children(actions.size());
  for (int c = 0; c < actions.size(); ++c) {
    State new_state(state);
    doAction(game, &actions[c], &new_state);
    children[c] = init_game_tree(actions[c], new_state, game, idx);
  }

  uint64_t info_idx = idx++;
  char buff[100];
  printState(game, &state, 100, buff);
  auto ret = new InformationSetNode(std::string(buff), info_idx, action,
                                    currentPlayer(game, &state), state.round,
                                    children);
  // char state_str[100];
  // printState(game, &state, 100, state_str);
  // std::cout << state_str << std::endl;
  // std::cout << ret << std::endl;
  return ret;
}

bool AbstractGame::do_intersect(card_c v1, card_c v2) {
  card_c intersect;
  std::sort(v1.begin(), v1.end());
  std::sort(v2.begin(), v2.end());
  std::set_intersection(v1.begin(), v1.end(), v2.begin(), v2.end(),
                        std::back_inserter(intersect));
  return intersect.size() > 0;
}

unsigned AbstractGame::find_index(card_c v1, vector<card_c> v2) {
  std::sort(v1.begin(), v1.end());
  for (unsigned i = 0; i < v2.size(); ++i) {
    card_c intersect;
    std::sort(v2[i].begin(), v2[i].end());
    std::set_intersection(v1.begin(), v1.end(), v2[i].begin(), v2[i].end(),
                          std::back_inserter(intersect));
    if (intersect.size() == v1.size()) return i;
  }
  throw std::runtime_error("could not find v1 in v2");
}

bool AbstractGame::check_eq(card_c v1, card_c v2) {
  if (v1.size() != v2.size()) {
    return false;
  }
  for (size_t i = 0; i != v1.size(); ++i) {
    if (v1[i] != v2[i]) {
      return false;
    }
  }
  return true;
}

INode *AbstractGame::init_public_tree(Action action,
                                      InformationSetNode *tree_node,
                                      State &state, uint64_t hands_idx,
                                      card_c board, card_c deck,
                                      const Game *game, uint64_t &idx,
                                      bool deal_holes, bool deal_board) {
  // root of tree, deal holes
  if (deal_holes) {
    PrivateChanceNode *c = new PrivateChanceNode(game->numHoleCards, hands_idx,
                                                 board, game, state);

    c->child =
        init_public_tree(action, tree_node, state, idx, board, deck, game, idx);
    return c;
  }

  if (deal_board) {
    PublicChanceNode *c = new PublicChanceNode(game->numBoardCards[state.round],
                                               hands_idx, board, game, state);

    // generate possible boards
    hand_list combinations = generate_combinations(
        deck.size(), game->numBoardCards[state.round], board);
    auto nb_combinations = combinations.size();
    std::cout << "nb of board combinations " << nb_combinations << std::endl;

    vector<INode *> children(nb_combinations);

// NOTE: only works for 2 round poker games.
#pragma omp parallel for
    for (unsigned i = 0; i < nb_combinations; ++i) {
      card_c newboard(board);
      card_c newdeck(deck);
      for (unsigned c = 0; c < combinations[i].size(); ++c) {
        int card = deck[combinations[i][c]];
        newdeck[combinations[i][c]] = newdeck[newdeck.size() - 1];
        newdeck.pop_back();
        newboard.push_back(card);
      }

      children[i] = init_public_tree(action, tree_node, state, hands_idx,
                                     newboard, newdeck, game, idx);
    }

    c->children = children;
    // char state_str[100];
    // printState(game, &state, 100, state_str);
    // std::cout << state_str << std::endl;
    // std::cout << c << std::endl;
    return c;
  }

  if (state.finished) {
    int money = state.spent[0];
    vector<vector<double>> payoffs;
    int fold_player = state.playerFolded[0] ? 0 : 1;
    int money_f = state.spent[fold_player];

    auto ph = deck_to_combinations(game->numHoleCards, deck);

    if (!(state.playerFolded[0] || state.playerFolded[1])) {
      return new ShowdownNode(action, money, hands_idx, board);
    } else {
      return new FoldNode(action, fold_player, money_f, hands_idx, board);
    }
  }

  const State *s = &state;
  InformationSetNode *game_node = (InformationSetNode *)lookup_state(
      s, currentPlayer(game, s), game_tree_root(), 0, 0);
  if (tree_node->get_idx() != game_node->get_idx()) {
    std::cout << tree_node->get_idx() << " " << game_node->get_idx()
              << std::endl;
    std::cout << tree_node->get_info_str() << " " << game_node->get_info_str()
              << std::endl;
    std::cerr << "error public tree node index" << std::endl;
  }
  std::vector<Action> actions = action_abs->get_actions(game, state);
  std::vector<INode *> children(actions.size());
  char buff[100];
  printState(game, &state, 100, buff);
  InformationSetNode *n = new InformationSetNode(
      std::string(buff), game_node->get_idx(), action,
      currentPlayer(game, &state), state.round, {}, hands_idx, board);
  int curr_round = state.round;

  for (int c = 0; c < actions.size(); ++c) {
    State new_state(state);
    doAction(game, &actions[c], &new_state);
    int new_round = new_state.round;
    children[c] = init_public_tree(
        actions[c], (InformationSetNode *)(tree_node->get_children()[c]),
        new_state, hands_idx, board, deck, game, idx, false,
        curr_round < new_round);
  }

  n->children = children;
  return n;
}

card_c AbstractGame::generate_deck(int ranks, int suits) {
  card_c deck(ranks * suits);
  int numCards = 0;
  for (int r = 0; r < ranks; ++r) {
    for (int s = 0; s < suits; ++s) {
      deck[numCards] = makeCard(r, s);
      ++numCards;
    }
  }
  return deck;
}

hand_list AbstractGame::generate_hole_combinations(int hand_size, card_c deck) {
  hand_list combinations = generate_combinations(deck.size(), hand_size, {});
  auto nb_combinations = combinations.size();

  hand_list hands(nb_combinations);
  for (unsigned c = 0; c < nb_combinations; ++c) {
    card_c combo(combinations[c].size());
    for (unsigned s = 0; s < combinations[c].size(); ++s) {
      combo[s] = deck[combinations[c][s]];
    }
    hands[c] = combo;
  }

  return hands;
}

INode *AbstractGame::game_tree_root() { return game_tree; }
INode *AbstractGame::public_tree_root() {
  if (!public_tree) {
    uint64_t idi = 0;
    card_c deck = generate_deck(game->numRanks, game->numSuits);
    State initial_state;
    initState(game, 0, &initial_state);
    public_tree =
        init_public_tree({a_invalid, 0}, (InformationSetNode *)game_tree_root(),
                         initial_state, idi, {}, deck, game, idi, true);
  }
  return public_tree;
}

void AbstractGame::print_gamedef() { printGame(stdout, game); }

// KUHN GAME
KuhnGame::KuhnGame(const Game *game_definition, CardAbstraction *cabs,
                   ActionAbstraction *aabs, int nb_threads)
    : AbstractGame(game_definition, cabs, aabs, nb_threads) {}

int KuhnGame::rank_hand(const card_c &hand, const card_c &board) {
  return rankOfCard(hand[0]);
}

void KuhnGame::evaluate(hand_t &hand) {
  hand.value[0] =
      rankOfCard(hand.holes[0][0]) > rankOfCard(hand.holes[1][0]) ? 1 : -1;
  hand.value[1] = hand.value[0] * -1;
}

// LEDUC
LeducGame::LeducGame(const Game *game_definition, CardAbstraction *cabs,
                     ActionAbstraction *aabs, int nb_threads)
    : AbstractGame(game_definition, cabs, aabs, nb_threads) {}

void LeducGame::evaluate(hand_t &hand) {
  int p1r = rank_hand(hand.holes[0][0], hand.board[0]);
  int p2r = rank_hand(hand.holes[1][0], hand.board[0]);

  if (p1r > p2r) {
    hand.value[0] = 1;
    hand.value[1] = -1;
  } else if (p1r < p2r) {
    hand.value[0] = -1;
    hand.value[1] = 1;
  } else {
    hand.value[0] = 0;
    hand.value[1] = 0;
  }
}

int LeducGame::rank_hand(const card_c &hand, const card_c &board) {
  return rank_hand(hand[0], board[0]);
}

int LeducGame::rank_hand(int hand, int board) {
  int h = rankOfCard(hand);
  int b = rankOfCard(board);

  if (h == b) return 100;
  return h;
}

HoldemGame::HoldemGame(const Game *game_definition, CardAbstraction *cabs,
                       ActionAbstraction *aabs, ecalc::Handranks *hr,
                       int nb_threads)
    : AbstractGame(game_definition, cabs, aabs, nb_threads), handranks(hr) {}

int HoldemGame::rank_hand(const card_c &hole, const card_c &board) {
  Cardset c1 = emptyCardset();
  for (int i = 0; i < hole.size(); ++i) {
    addCardToCardset(&c1, suitOfCard(hole[i]), rankOfCard(hole[i]));
  }
  for (int i = 0; i < board.size(); ++i) {
    addCardToCardset(&c1, suitOfCard(board[i]), rankOfCard(board[i]));
  }
  return rankCardset(c1);
}

void HoldemGame::evaluate(hand_t &hand) {
  int p1r = rank_hand(hand.holes[0], hand.board);
  int p2r = rank_hand(hand.holes[1], hand.board);

  if (p1r > p2r) {
    hand.value[0] = 1;
    hand.value[1] = -1;
  } else if (p1r < p2r) {
    hand.value[0] = -1;
    hand.value[1] = 1;
  } else {
    hand.value[0] = 0;
    hand.value[1] = 0;
  }
}
