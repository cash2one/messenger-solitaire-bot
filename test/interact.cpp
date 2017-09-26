#include <unistd.h>

#include <robot.h>

#include "interact.hpp"
#include "vision.hpp"
#include "game.hpp"

static bool sandbox = false;
static robot_h robot;

static void click_card(uint32_t x, uint32_t y)
{
  robot_mouse_move(robot, x, y);
  robot_mouse_press(robot, ROBOT_BUTTON1_MASK);
  robot_mouse_release(robot, ROBOT_BUTTON1_MASK);

  /* TODO(fyquah): This isn't super reliable, as it is decided based on
   * the processor's (underterministic) speed.
   */
  usleep(200000);  /* Microseconds */
}

static void drag_mouse(
    std::pair<uint32_t, uint32_t> from,
    std::pair<uint32_t, uint32_t> to
)
{
  robot_mouse_move(robot, from.first, from.second);
  robot_mouse_press(robot, ROBOT_BUTTON1_MASK);
  robot_mouse_move(robot, to.first, to.second);
  robot_mouse_release(robot, ROBOT_BUTTON1_MASK);
  usleep(200000);
}

static void unsafe_remove_card_from_visible_pile(game_state_t * state)
{
  state->remaining_pile_size = state->remaining_pile_size - 1;

  if (state->remaining_pile_size == state->stock_pile_size) {
    state->waste_pile_top = Option<card_t>();
  } else {
    state->waste_pile_top = Option<card_t>(recognize_visible_pile_card());
  }
}

void interact_init(robot_h r)
{
  robot = r;
}

void set_sandbox_mode(bool a)
{
  sandbox = a;
}

game_state_t load_initial_game_state()
{
  tableau_deck_t tableau[7];
  uint32_t stock_pile_size = 24;

  for (uint32_t i = 0 ; i < 7 ; i++) {
    tableau_position_t pos = { .deck = i, .num_hidden = i, .position = 0 };
    tableau[i].num_down_cards = i;
    tableau[i].cards = { recognize_tableau_card(pos) };
  }
  auto none = Option<card_t>();

  /* A sane compiler should be able to inline this at the call site */
  game_state_t ret;
  for (int i = 0 ; i < 4 ; i++) {
    ret.foundation[i] = none;
  }
  ret.waste_pile_top = none;
  for (int i = 0; i < 7; i++) {
    ret.tableau[i] = tableau[i];
  }
  ret.stock_pile_size = stock_pile_size;
  ret.remaining_pile_size = stock_pile_size;

  return ret;
}

game_state_t draw_from_stock_pile(const game_state_t & state)
{
  game_state_t next_state = state;

  if (state.stock_pile_size <= 0) {
    throw IllegalMoveException();
  }

  click_card(
      DRAW_PILE.first + CARD_WIDTH / 2,
      DRAW_PILE.second + CARD_HEIGHT / 2
  );

  next_state.stock_pile_size -= 1;
  next_state.waste_pile_top = Option<card_t>(recognize_visible_pile_card());

  return next_state;
}

game_state_t reset_stock_pile(const game_state_t & state)
{
  game_state_t next_state = state;

  if (state.stock_pile_size != 0) {
    throw IllegalMoveException();
  }

  click_card(
      DRAW_PILE.first + CARD_WIDTH / 2,
      DRAW_PILE.second + CARD_HEIGHT / 2
  );

  next_state.stock_pile_size = state.remaining_pile_size;
  next_state.waste_pile_top = Option<card_t>();

  return next_state;
}

static bool is_transfer_legal(
    const card_t & card, const tableau_deck_t & deck)
{
  if (deck.num_down_cards == 0
      && deck.cards.size() == 0
      && card.number == KING) {
    return true;

  } else if (deck.cards.back().number - 1 == card.number
      && (deck.cards.back().suite % 2) != (card.suite % 2)) {
    return true;

  } else {
    return false;
  }
}

static bool is_promote_to_foundation_legal(
    const Option<card_t> foundation,
    const card_t & card)
{
  return (
      (!foundation.is_some() && card.number == ACE)
      || (foundation.is_some()
          && foundation.get().suite == card.suite
          && foundation.get().suite == card.number - 1)
  );
}

game_state_t move_from_visible_pile_to_tableau(
    const game_state_t & state,
    const uint32_t deck
)
{
  if (!state.waste_pile_top.is_some()) {
    throw IllegalMoveException();
  }

  const card_t & waste_pile_top = state.waste_pile_top.get();
  const tableau_deck_t & tableau_deck = state.tableau[deck];

  if (!is_transfer_legal(waste_pile_top, tableau_deck)) {
    throw IllegalMoveException();
  }

  /* Dragging the card in the game */
  std::pair<uint32_t, uint32_t> from = std::make_pair(
      VISIBLE_PILE.first + CARD_WIDTH / 2 ,
      VISIBLE_PILE.second + CARD_HEIGHT / 2
  );
  std::pair<uint32_t, uint32_t> to = std::make_pair(
    TABLEAU.first
      + (deck * TABLEAU_SIDE_OFFSET)
      + (CARD_WIDTH / 2),
    TABLEAU.second
      + (tableau_deck.num_down_cards * TABLEAU_UNSEEN_OFFSET)
      + ((tableau_deck.cards.size() ? tableau_deck.cards.size() - 1 : 0) * TABLEAU_SEEN_OFFSET)
      + (CARD_HEIGHT / 2)
  );
  drag_mouse(from, to);

  game_state_t next_state = state;

  next_state.tableau[deck].cards.push_back(waste_pile_top);

  /* See the new card in the pile */
  unsafe_remove_card_from_visible_pile(&next_state);

  return next_state;
}

game_state_t move_from_visible_pile_to_foundation(
    const game_state_t & state,
    const uint32_t foundation_pos
)
{
  if (!state.waste_pile_top.is_some()) {
    throw IllegalMoveException();
  }

  const Option<card_t> & foundation = state.foundation[foundation_pos];
  const card_t & waste_pile_top = state.waste_pile_top.get();

  if (!is_promote_to_foundation_legal(foundation, waste_pile_top)) {
    throw IllegalMoveException();
  }

  std::pair<uint32_t, uint32_t> from = std::make_pair(
      VISIBLE_PILE.first + CARD_WIDTH / 2 ,
      VISIBLE_PILE.second + CARD_HEIGHT / 2
  );
  std::pair<uint32_t, uint32_t> to = std::make_pair(
      FOUNDATION_DECKS[foundation_pos].first + CARD_WIDTH / 2 ,
      FOUNDATION_DECKS[foundation_pos].second + CARD_HEIGHT / 2
  );
  drag_mouse(from, to);

  game_state_t next_state = state;
  next_state.foundation[foundation_pos] = Option<card_t>(waste_pile_top);
  unsafe_remove_card_from_visible_pile(&next_state);

  return next_state;
}
