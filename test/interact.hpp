#ifndef INTERACT_HPP
#define INTERACT_HPP

#include <exception>

#include "game.hpp"

class IllegalMoveException : public std::exception
{
};

void interact_init(robot_h robot);
void set_sandbox_mode(bool flag);

game_state_t load_initial_game_state();
game_state_t draw_from_stock_pile(const game_state_t &);
game_state_t reset_stock_pile(const game_state_t &);

game_state_t move_from_visible_pile_to_tableau(
    const game_state_t &,
    const uint32_t deck
);
game_state_t move_from_visible_pile_to_foundation(
    const game_state_t &,
    const uint32_t deck
);
game_state_t move_from_tableau_to_foundation(
    const game_state_t &,
    const uint32_t tableau_position,
    const uint32_t destination
);
game_state_t move_from_column_to_column(
    const game_state_t & game,
    const tableau_position_t & position,
    const uint32_t destination
);

#endif