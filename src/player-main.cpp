#include <boost/program_options.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include "cfrm.hpp"
#include "functions.hpp"
#include "main_functions.hpp"

extern "C" {
#include "net.h"
}

#define CFR_SAMPLER ChanceSamplingCFR

void threshold_strategy(std::vector<double> &strategy, double threshold) {
  double sum = 0;
  for (unsigned i = 0; i < strategy.size(); ++i) {
    if (strategy[i] < threshold) strategy[i] = 0;
    sum += strategy[i];
  }

  for (unsigned i = 0; i < strategy.size(); ++i) {
    strategy[i] = strategy[i] == 0 ? 0 : (strategy[i] / sum);
  }
}

// threshold = max distance between two probabilities to be considered equal
void purify_strategy(std::vector<double> &strategy, double threshold = 0.001) {
  double max_prob = *std::max_element(strategy.begin(), strategy.end());
  double sum = 0;
  for (unsigned i = 0; i < strategy.size(); ++i) {
    if ((max_prob - strategy[i]) < threshold)
      strategy[i] = 1;
    else
      strategy[i] = 0;
    sum += strategy[i];
  }

  for (unsigned i = 0; i < strategy.size(); ++i) {
    strategy[i] = strategy[i] == 0 ? 0 : (strategy[i] / sum);
  }
}

using namespace std;
namespace ch = std::chrono;
namespace po = boost::program_options;

int parse_options(int argc, char **argv);
void read_game(char *game_definition);

struct {
  game_t type = leduc;
  string game_definition = "games/holdem.limit.2p.reverse_blinds.game";

  string host = "localhost";
  unsigned port = 18791;

  unsigned seed = 0;

  card_abstraction card_abs = NULLCARD_ABS;
  action_abstraction action_abs = NULLACTION_ABS;
  string card_abs_param = "";
  string action_abs_param = "";

  string init_strategy = "";

  double threshold = -1;
  double purify = -1;
} options;

const Game *gamedef;

int main(int argc, char **argv) {
  int sock, len, r, a;
  int32_t min, max;
  uint16_t port;
  double p;
  MatchState state;
  Action action;
  FILE *file, *toServer, *fromServer;
  struct timeval tv;
  double probs[NUM_ACTION_TYPES];
  double actionProbs[NUM_ACTION_TYPES];
  char line[MAX_LINE_LEN];

  /* Initialize the player's random number state using time */

  if (parse_options(argc, argv) == 1) return 1;

  nbgen rng(options.seed);
  read_game((char *)options.game_definition.c_str());

  if (options.threshold > 0)
    cout << "using thresholding with param: " << options.threshold << ""
         << std::endl;
  ;

  cout << "using information abstraction type: "
       << card_abstraction_str[options.card_abs]
       << " with parameter: " << options.card_abs_param << "" << std::endl;
  ;
  CardAbstraction *card_abs =
      load_card_abstraction(gamedef, options.card_abs, options.card_abs_param);

  cout << "using action abstraction type: "
       << action_abstraction_str[options.action_abs]
       << " with parameter: " << options.action_abs_param << "" << std::endl;
  ;
  ActionAbstraction *action_abs = load_action_abstraction(
      gamedef, options.action_abs, options.action_abs_param);
  AbstractGame *agame;
  switch (options.type) {
    case kuhn:
      agame = new KuhnGame(gamedef, card_abs, action_abs);
      break;
    case leduc:
      agame = new LeducGame(gamedef, card_abs, action_abs);
      break;
    case holdem:
      agame = new HoldemGame(gamedef, card_abs, action_abs, NULL);
      break;
  };
  cout << "created holdem game tree." << std::endl;
  ;
  CFRM *cfr = new CFR_SAMPLER(agame, (char *)options.init_strategy.c_str());
  cout << "CFR Initialized" << std::endl;
  ;
  std::cout << "Number of informationsets:" << agame->get_nb_infosets() << ""
            << std::endl;
  ;
  std::cout << "Number of terminalnodes:"
            << cfr->count_terminal_nodes(agame->game_tree_root()) << ""
            << std::endl;
  ;

  /* connect to the dealer */
  sock = connectTo((char *)options.host.c_str(), options.port);
  if (sock < 0) {
    std::cout << "could not connect to socket" << std::endl;
    ;
    exit(EXIT_FAILURE);
  }
  toServer = fdopen(sock, "w");
  fromServer = fdopen(sock, "r");
  if (toServer == NULL || fromServer == NULL) {
    fprintf(stderr, "ERROR: could not get socket streams\n");
    exit(EXIT_FAILURE);
  }

  //////[> send version string to dealer <]
  if (fprintf(toServer, "VERSION:%" PRIu32 ".%" PRIu32 ".%" PRIu32 "\n",
              VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION) != 14) {
    fprintf(stderr, "ERROR: could not get send version to server\n");
    exit(EXIT_FAILURE);
  }
  fflush(toServer);

  // play the gamedef!
  while (fgets(line, MAX_LINE_LEN, fromServer)) {
    // char *line = "MATCHSTATE:0:112:cr6c/cr28:8d9s|/2hJh6h";
    // char *line = "MATCHSTATE:1:33:c:|Qs8h";
    // char* line = "MATCHSTATE:0:1229:cr10c/r28c/r50r126:5sQd|/8s8cAs/5c";
    printf("%s\n", line);

    /* ignore comments */
    if (line[0] == '#' || line[0] == ';') {
      continue;
    }

    len = readMatchState(line, gamedef, &state);
    if (len < 0) {
      fprintf(stderr, "ERROR: could not read state %s", line);
      exit(EXIT_FAILURE);
    }

    if (stateFinished(&state.state)) {
      //[> ignore the gamedef over message <]
      continue;
    }

    if (currentPlayer(gamedef, &state.state) != state.viewingPlayer) {
      //[> we're not acting <]
      continue;
    }

    // add a colon (guaranteed to fit because we read a new-line in fgets)
    line[len] = ':';
    ++len;

    // char *c = new char[300];
    // printState(gamedef, &state.state, 300, c);
    // printf("%s\n", c);

    // lookup current node we are in
    InformationSetNode *curr_node = (InformationSetNode *)cfr->lookup_state(
        &state.state, state.viewingPlayer);
    // std::cout << "i am behind lookup" << std::endl;;

    // CHECK IF WE FOUND THE CORRECT NODE
    if (curr_node != NULL) {
      // std::cout << "lookup was successful";
      // get strategy at curr_node
      //
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
        std::cout << card_str << ":";
      }
      if (board.size()) {
        printCards(board.size(), &(board[0]), 100, card_str);
        std::cout << card_str << std::endl;
      }

      auto strategy = cfr->get_normalized_avg_strategy(
          curr_node->get_idx(), hand, board, state.state.round);

      // cout << "prev: ";
      // for(unsigned i = 0; i < strategy.size();++i)
      // cout << strategy[i] << " ";
      // cout << "" << std::endl;;

      if (options.threshold > 0) {
        threshold_strategy(strategy, options.threshold);
      }

      if (options.purify > 0) {
        purify_strategy(strategy, options.purify);
      }
      // cout << "thresholded: ";
      // for(unsigned i = 0; i < strategy.size();++i)
      // cout << strategy[i] << " ";
      // cout << "" << std::endl;;

      // choose according to distribution
      std::discrete_distribution<int> d(strategy.begin(), strategy.end());
      int max_tries = 10;
      do {
        int action_idx = d(rng);
        action = curr_node->get_children()[action_idx]->get_action();
        --max_tries;
        // std::cout << "#" << max_tries
        //<< " choosen action: " << ActionsStr[action.type] << " = "
        //<< action.size << "" << std::endl;;
        if (isValidAction(gamedef, &state.state, 0, &action)) break;
      } while (max_tries > 0);
      // after max tries no answer was found. jump into the fail branch.
      if (!isValidAction(gamedef, &state.state, 0, &action)) goto LASTRESCUE;
    } else {
    LASTRESCUE:
      std::cout
          << "state not found in game tree. forcing first possible action."
          << std::endl;
      ;
      for (a = 0; a < NUM_ACTION_TYPES; ++a) {
        action.type = (ActionType)a;
        if (isValidAction(gamedef, &state.state, 0, &action)) break;
      }
      cout << "choosen action is " << ActionsStr[action.type] << ""
           << std::endl;
      ;
    }

    //[> do the action! <]
    assert(isValidAction(gamedef, &state.state, 0, &action));
    cout << "choosen action: " << ActionsStr[action.type] << "= " << action.size
         << "" << std::endl;
    ;
    r = printAction(gamedef, &action, MAX_LINE_LEN - len - 2, &line[len]);
    if (r < 0) {
      fprintf(stderr, "ERROR: line too long after printing action\n");
      exit(EXIT_FAILURE);
    }
    len += r;
    line[len] = '\r';
    ++len;
    line[len] = '\n';
    ++len;

    if (fwrite(line, 1, len, toServer) != len) {
      fprintf(stderr, "ERROR: could not get send response to server\n");
      exit(EXIT_FAILURE);
    }
    fflush(toServer);
  }

  return 0;
}

int parse_options(int argc, char **argv) {
  try {
    po::options_description desc("Allowed options");
    desc.add_options()("help,h", "produce help message")(
        "game-type,t", po::value<string>(), "kuhn,leduc,holdem")(
        "card-abstraction,c", po::value<string>(),
        "set card abstraction to use")("action-abstraction,a",
                                       po::value<string>(),
                                       "set action abstraction to use")(
        "action-abstraction-param,n",
        po::value<string>(&options.action_abs_param),
        "parameter passed to the action abstraction.")(
        "card-abs-param,m", po::value<string>(&options.card_abs_param),
        "parameter for card abstraction")(
        "host,o", po::value<string>(&options.host), "host to connect to")(
        "port,p", po::value<unsigned>(&options.port), "port to connect to")(
        "init-strategy,i", po::value<string>(&options.init_strategy),
        "initialize regrets and avg strategy from file")(
        "gamedef,g", po::value<string>(&options.game_definition),
        "gamedefinition to use")(
        "threshold,t", po::value<double>(&options.threshold),
        "Set this if thresholding should be applied to retrieved strategies.")(
        "purify,u", po::value<double>(&options.purify),
        "Set this if purification should be applied to strategies. parameter "
        "determines how far"
        "2 probabilities can be apart to be still considered equal.");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("game-type")) {
      string gt = vm["game-type"].as<string>();
      if (gt == "kuhn")
        options.type = kuhn;
      else if (gt == "leduc")
        options.type = leduc;
      else if (gt == "holdem")
        options.type = holdem;
    }

    if (vm.count("card-abstraction")) {
      string ca = vm["card-abstraction"].as<string>();
      if (ca == "null")
        options.card_abs = NULLCARD_ABS;
      else if (ca == "cluster")
        options.card_abs = CLUSTERCARD_ABS;
    }

    if (vm.count("action-abstraction")) {
      string ca = vm["action-abstraction"].as<string>();
      if (ca == "null")
        options.action_abs = NULLACTION_ABS;
      else if (ca == "potrel")
        options.action_abs = POTRELACTION_ABS;
    }

    if (vm.count("help")) {
      cout << desc << "" << std::endl;
      ;
      return 1;
    }
  } catch (exception &e) {
    std::cout << "unable to parse parameters" << std::endl;
    return 1;
  } catch (...) {
  }
  return 0;
}

void read_game(char *game_definition) {
  FILE *file = fopen(game_definition, "r");
  if (file == NULL) {
    std::cout << "could not read game file" << std::endl;
    ;
    exit(-1);
  }
  gamedef = readGame(file);
  if (gamedef == NULL) {
    std::cout << "could not parse game file" << std::endl;
    ;
    exit(-1);
  }
}
