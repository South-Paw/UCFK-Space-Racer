/**
 * game.c
 *
 * Copyright 2015 James Paterson and Alex Gabites
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "system.h"
#include "pio.h"
#include "pacer.h"
#include "tinygl.h"
#include "navswitch.h"
#include "led.h"
#include "ir_uart.h"
#include "../fonts/font5x7_2.h"

#define MAP_WIDTH 5
#define MAP_HEIGHT 8 // An extra line is created for off screen mine storage

// Speed of game updates in ms (1000 ms = 1 second)
#define REFRESH_RATE 1000

// Global player dead state
static uint8_t dead = 0;

/**
 * For IR reading.
 * @return got The character received or 'N' if nothing was recieved.
 */
char ir_read(void) {
	char got = 'N';

	if (ir_uart_read_ready_p()) {
		got = ir_uart_getc();
	}

	return got;
}

/**
 * For sending a character;
 *  - bomb: transmit char of uint8_t of column index ie; `2`
 *  - player death: transmit 'D'
 * @param send The character to be sent.
 */
void ir_write(char send) {
	ir_uart_putc(send);
}

/**
 * For uint8_tial setup.
 * Send 'C' character and check for recieving 'C'
 * When we recieve 'C' we're connected so move to next state.
 * @return 0 if no connection or 1 if connected
 */
uint8_t ir_connect(void) {
	char recieved;

	ir_write('C');

	recieved = ir_read();

	return recieved;
}

/**
 * Clears the map matrix.
 * @param map[][] The map matrix.
 */
void clear_map(uint8_t map[MAP_WIDTH][MAP_HEIGHT]) {
	uint8_t temp_var, temp_var2;

	for (temp_var = 0; temp_var < MAP_WIDTH; temp_var++) {
		for (temp_var2 = 0; temp_var2 < MAP_HEIGHT; temp_var2++){
			map[temp_var][temp_var2] = 0;
		}
	}
}

/**
 * Clears the mines.
 * @param mines[] The mines matrix.
 */
void clear_mines(int mines[MAP_WIDTH - 1]) {
	uint8_t temp_var;

	for (temp_var = 0; temp_var < MAP_WIDTH - 1; temp_var++) {
		mines[temp_var] = -1;
	}
}

/**
 * Set that the player died.
 */
void player_died(void) {
	dead = 1;
}

/**
 * Detect a collision and issue a death if needed.
 * @param map[][] The map matrix.
 * @param newpos
 */
uint8_t collision(uint8_t map[MAP_WIDTH][MAP_HEIGHT], tinygl_point_t newpos) {
	if (newpos.x > 3 || newpos.x < 0) {
		return 1;
	} else if (map[newpos.x][0] || map[newpos.x][1]) {
		player_died();
		return 1;
	} else if (map[newpos.x + 1][0] || map[newpos.x + 1][1]) {
		player_died();
		return 1;
	} else {
		return 0;
	}
}

/**
 * Draw the bombs on the screen.
 * @param mines[] The mines matrix.
 */
void draw_bombs(int mines[MAP_WIDTH - 1]) {
	tinygl_point_t bomb, bomb_op; // Declares bomb boundry point8_ts
	uint8_t temp_var;

	for (temp_var = 0; temp_var < MAP_WIDTH - 1; temp_var++) {
		if (mines[temp_var] + 1){
			bomb = tinygl_point(temp_var, 6 - mines[temp_var]);
			bomb_op = tinygl_point(temp_var + 1, mines[temp_var] - 1 < 0 ? 6 :(7 - mines[temp_var]));
			tinygl_draw_box(bomb, bomb_op, 1);
		}
	}
}

/**
 * Draw a skull.
 */
void draw_skull(void){
	tinygl_draw_box(tinygl_point(3,2),tinygl_point(1,3),1);
	tinygl_draw_line(tinygl_point(3,0),tinygl_point(1,0),1);
	tinygl_draw_line(tinygl_point(4,1),tinygl_point(4,2),1);
	tinygl_draw_line(tinygl_point(0,1),tinygl_point(0,2),1);
	tinygl_draw_line(tinygl_point(1,5),tinygl_point(3,5),1);
	tinygl_pixel_set(tinygl_point(2,1), 1);
	tinygl_pixel_set(tinygl_point(0,6), 1);
	tinygl_pixel_set(tinygl_point(0,4), 1);
	tinygl_pixel_set(tinygl_point(4,6), 1);
	tinygl_pixel_set(tinygl_point(4,4), 1);
}

/**
 * Draw a cup
 */
void draw_cup(void){
	tinygl_draw_box(tinygl_point(4,1),tinygl_point(0,2),1);
	tinygl_draw_line(tinygl_point(3,3),tinygl_point(1,5),1);
	tinygl_draw_line(tinygl_point(2,3),tinygl_point(2,5),1);
	tinygl_draw_line(tinygl_point(1,3),tinygl_point(3,5),1);
}

int main(void) {
	/* Game states
	 *	9 = menu
	 *	0 = connection setup
	 *	1 = countdown
	 *	2 = playing
	 *	3 = game over
	 */
	uint8_t game_state = 9;

	// Map matrix to store screen contents.
	uint8_t map[MAP_WIDTH][MAP_HEIGHT];

	// Mines matrix
	int mines[MAP_WIDTH-1] = {-1,-1,-1,-1};

	// Connection and multiplayer.
	uint8_t connection = 'N';
	uint8_t multiplayer = 0;

	// Player cords
	uint8_t playerx = 2;
	uint8_t playery = 1;

	// Players box
	tinygl_point_t player = tinygl_point(playerx, 6 - playery);
	tinygl_point_t player_op = tinygl_point(playerx + 1 ,7 - playery);

	//Game speed. Represents mine drops in Hz
	uint8_t speed = 2;

	// Iterators for counting during states.
	uint8_t temp_var = 0; // temp_var: Used with a cycle of the while loop
	uint16_t count = 0; // count: Persists over multiple cycles

	// Frequency of game ticks
	int cycle_size = REFRESH_RATE;

	char* counter;

	// System setup
	system_init();
	ir_uart_init();
	pacer_init(REFRESH_RATE);
	tinygl_init(REFRESH_RATE);

	tinygl_font_set(&font5x7_2);
	tinygl_text_speed_set(10);
	tinygl_text_dir_set(TINYGL_TEXT_DIR_NORMAL);

	clear_map(map);

	while (1) {
		pacer_wait();
		navswitch_update();

		switch (game_state) {
			case 9: // Menu
				// pixels that are always on
				tinygl_pixel_set(tinygl_point(2,1), 1); // middle arrow
				tinygl_pixel_set(tinygl_point(3,5), 1); // 1p
				tinygl_pixel_set(tinygl_point(1,4), 1); // 2p
				tinygl_pixel_set(tinygl_point(1,6), 1); // 2p

				if (!multiplayer) { // singleplayer selected
					tinygl_pixel_set(tinygl_point(4,1), 1);
					tinygl_pixel_set(tinygl_point(3,2), 1);
					tinygl_pixel_set(tinygl_point(1,2), 0);
					tinygl_pixel_set(tinygl_point(0,1), 0);

					if (navswitch_push_event_p(NAVSWITCH_WEST)) {
						multiplayer = 1;
					}
				} else { // multiplayer selected
					tinygl_pixel_set(tinygl_point(1,2), 1);
					tinygl_pixel_set(tinygl_point(0,1), 1);
					tinygl_pixel_set(tinygl_point(4,1), 0);
					tinygl_pixel_set(tinygl_point(3,2), 0);

					if (navswitch_push_event_p(NAVSWITCH_EAST)) {
						multiplayer = 0;
					}
				}

				if (navswitch_push_event_p(NAVSWITCH_PUSH)) {
					tinygl_clear();
					if (multiplayer) {
						game_state = 0;
					} else {
						game_state = 1;
					}
				}
				break;

			case 0: // Connection
				connection = ir_connect();
				if (connection == 'C') {
					game_state = 1;
				}
				break;

			case 1: // Pre-game countdown
				if (count < 3000) {
					counter = "3\0";
					counter[0] -= count / 1000;
					tinygl_text(counter);
				} else {
					count = 0;
					game_state = 2;
				}
				count++;
				break;

			case 2: // Game loop
				tinygl_clear();

				if (multiplayer){
					connection = ir_read();
				}

				// Drop mines
				if (count > (cycle_size / speed)) {
					for (temp_var = 0; temp_var < (MAP_WIDTH - 1); temp_var++) {
						if (mines[temp_var] + 1) {
							map[temp_var][mines[temp_var]] = 0;
							map[temp_var + 1][mines[temp_var]] = 0;

							mines[temp_var] -= 1;

							if (mines[temp_var] > 0) {
								map[temp_var][mines[temp_var] - 1] = 1;
								map[temp_var + 1][mines[temp_var] - 1] = 1;
							}
						}

						if (mines[temp_var] + 1 && mines[temp_var] < 3) {
							if (temp_var == playerx || temp_var == (playerx + 1) || (temp_var + 1) == playerx) {
								player_died();
							}
						}

						count = 0;

						// Increases game speed until it gets to 10 times speed.
						// Increase 10 for a faster/harder game, decrease for slower/easier
						if (cycle_size > REFRESH_RATE / 10) {
							cycle_size -= 1;
						}
					}
				}

				count++;

				if (multiplayer) {
					if (navswitch_push_event_p(NAVSWITCH_PUSH)) {
						//Send playerx as message for where to drop mine
						ir_write('0' + playerx);
					}

					if (connection > ('0' - 1) && connection < ('0' + MAP_WIDTH - 1)) {
						connection -= '0';
						for (temp_var = 0; temp_var < MAP_WIDTH - 1; temp_var++) {
							if (mines[temp_var] > MAP_HEIGHT - 5){
								break;
							}
						}

						if (temp_var == MAP_WIDTH - 1){
							mines[connection] = 7;
							map[connection][7] = 1;
							map[connection][6] = 1;
							map[connection + 1][7] = 1;
							map[connection + 1][6] = 1;
						}
					}
				} else {
					for (temp_var = 0; temp_var < MAP_WIDTH - 1; temp_var++) {
						if (mines[temp_var] > MAP_HEIGHT - 5) {
							break;
						} else if (temp_var == playerx && mines[temp_var] != -1) {
							break;
						}
					}

					if (temp_var == MAP_WIDTH - 1) {
						mines[playerx] = 7;
						map[playerx][7] = 1;
						map[playerx][6] = 1;
						map[playerx + 1][7] = 1;
						map[playerx + 1][6] = 1;
					}
				}

				if (navswitch_push_event_p(NAVSWITCH_EAST)){ // Move right
					if (!collision(map, tinygl_point(playerx + 1, 1))) {
						playerx += 1;
					}

					player = tinygl_point(playerx, 6 - playery);
					player_op = tinygl_point(playerx + 1, 7 - playery);
				}
				if (navswitch_push_event_p(NAVSWITCH_WEST)){ // Move left
					if (!collision(map, tinygl_point(playerx - 1, 1))) {
						playerx -= 1;
					}

					player = tinygl_point(playerx, 6 - playery);
					player_op = tinygl_point(playerx + 1, 7 - playery);
				}

				draw_bombs(mines);

				tinygl_draw_box(player,player_op, 1);

				if (dead || connection == 'D'){
					if (dead) {
						// If this player dead, inform other that you've died
						ir_write('D');
					}

					tinygl_clear();
					game_state = 3;
				} else {
					break;
				}

			case 3:
				if (dead) {
					// if this player died
					// display skull and cross bones
					draw_skull();
				} else {
					// else if this player won
					// display a trophy/cup
					draw_cup();
				}

				if (navswitch_push_event_p(NAVSWITCH_PUSH)) { //Restart game and return to main menu
					game_state = 9;
					dead = 0;

					tinygl_clear();
					clear_map(map);
					clear_mines(mines);

					cycle_size = REFRESH_RATE;
					playery = 1;
				}
		}

		tinygl_update();
	}
}
