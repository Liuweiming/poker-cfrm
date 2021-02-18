#ifndef DEFINITIONS_HPP
#define DEFINITIONS_HPP

#include <inttypes.h>
#include <atomic>
#include <ecalc/xorshift_generator.hpp>
#include <limits>
#include <vector>

#include "entry.hpp"

extern "C" {
#include "game.h"
}

enum game_t { kuhn, leduc, holdem };

enum action_abstraction { NULLACTION_ABS, POTRELACTION_ABS };

static const char *action_abstraction_str[] = {"NULL", "POTREL"};

enum card_abstraction { CLUSTERCARD_ABS, NULLCARD_ABS, BLINDCARD_ABS };

static const char *card_abstraction_str[] = {"CLUSTER", "NULL", "BLIND"};

const int MAX_ABSTRACT_ACTIONS = 20;

const double DOUBLE_MAX = std::numeric_limits<double>::max();

static const char *ActionsStr[] = {"F", "C", "R", ""};

typedef int _Bool;  // needed by some c includes

class atomic_double : std::atomic<double> {
 public:
  using std::atomic<double>::atomic;
  using std::atomic<double>::operator=;

  atomic_double& operator+=(double v) {
    double old = load(std::memory_order_relaxed);
    while (!compare_exchange_weak(old, old + v, std::memory_order_release,
                                  std::memory_order_relaxed))
      ;
  }
  //implicit conversion
  operator double() const { return load(std::memory_order_relaxed); }
  
};

typedef Entry<atomic_double> entry_t;

typedef std::vector<uint8_t> card_c;
typedef std::vector<int> int_c;
typedef std::vector<double> dbl_c;
typedef std::vector<Action> action_c;
typedef std::vector<entry_t> entry_c;
typedef std::vector<card_c> hand_list;

typedef ecalc::XOrShiftGenerator nbgen;

struct hand_t {
  // +1 win, 0 tie, -1 lost per player
  typedef std::vector<int8_t> result;

  card_c board;
  result value;
  hand_list holes;

  hand_t() {}
  ~hand_t() {}

  hand_t(hand_list holes, card_c board)
      : value(holes.size()), holes(holes), board(board) {}

  hand_t(const hand_t &oh)
      : board(oh.board), holes(oh.holes), value(oh.value) {}

  hand_t operator=(const hand_t &h) {
    board = h.board;
    holes = h.holes;
    value = h.value;
    return *this;
  }
};

template <typename T, typename U>
std::ostream& operator<<(std::ostream& out, const std::pair<T, U>& v) {
  out << "{" << v.first << ", " << v.second << "}";
  return out;
}

template <typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<T>& v) {
  out << "[";
  for (int i = 0; i != v.size(); ++i) {
    out << v[i] << ((i == (v.size() - 1)) ? "" : " ");
  }
  out << "]";
  return out;
}

#endif
