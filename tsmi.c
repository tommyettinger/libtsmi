/***************************************************************************************************
    Copyright 2011 Lewis Potter

    This file is part of libtsmi.

    libtsmi is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libtsmi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libtsmi.  If not, see <http://www.gnu.org/licenses/>.
***************************************************************************************************/

#include <assert.h>
#include <glib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libtsmi.h"

/********************************
    INITIALISATIONS && INPUT
********************************/

const Coord DIRECTIONS[8] = {NORTH, NORTHEAST, EAST, SOUTHEAST, SOUTH, SOUTHWEST, WEST, NORTHWEST};
const int NUM_DIRECTIONS = 8; //Seems pritty stupid, but otherwise I'd just '8' everywhere.

static TCOD_random_t SEED; 

static int SCREEN_W;
static int SCREEN_H;

//If I try and initialise values that rely on unintialised values, lols will happen
bool screen_globals_init_p = false;

//So I can initialise all these values once rather than constantly passing them as paramaters from
//outside the lib.
void init_screen_globals(int screen_w, int screen_h) {
    SCREEN_W = screen_w;
    SCREEN_H = screen_h;
    screen_globals_init_p = true;
}

void init_game (int window_h, int window_w, char* title, int frames) {
    //Sanity checks
    assert(screen_globals_init_p);
    assert((window_w >= SCREEN_W) && (window_h >= SCREEN_H));
    
    SEED = TCOD_random_get_instance(); //I guess now would be a good time lol
    TCOD_console_set_keyboard_repeat(1, (1000/frames)); //so no delay when holding down a key.
   
    TCOD_console_init_root(window_w, window_h, title, false);
    TCOD_sys_set_fps(frames); 
}

/*
void clean_up() {
    //TODO: some way of cleaning up TileCommons created in host language
    free(GAME_W||LD);
}
*/ 
//Just a helper, saves me trying to figure out what TCOD_KEY_PRESSED is from the host
void check_key() {
    TCOD_console_check_for_keypress(TCOD_KEY_PRESSED);
}

/******************
    COORDINATES
******************/

static Coord* create_coord(guint x, guint y) {
    Coord* c = malloc(sizeof(Coord));
    c->x = x;
    c->y = y;
    
    return c;
}

Coord add_coords(Coord a, Coord b) {
    Coord c = {(a.x + b.x), (a.y + b.y)};
    return c;
}

/***************
    CREATURE
***************/

bool creature_move(Creature* c, int x_delta, int y_delta) {
    int dest_x = c->x + x_delta;
    int dest_y = c->y + y_delta;
    
    if (walkable_p(c->current_level, dest_x, dest_y)) {
        c->x = dest_x;
        c->y = dest_y;
        
        return true;
    } else {
        return false;
    }
}

//Maybe change this to bool if I can think of a situation where you can't turn
void creature_turn(Creature* c, bool turn_left) {
    //Using modulo 8 for wrap around.
    if (turn_left) {
        c->direction = (c->direction - 1) % 8;
    }
    else {
        c->direction = (c->direction + 1) % 8;
    }    
}


int creature_get_x(Creature* c) {
    return c->x;
}

int creature_get_y(Creature* c) {
    return c->y;
}

void creature_set_level(Creature* c, Level* l) {
    c->current_level = l;
}

Level* creature_get_level(Creature* c) {
    return c->current_level;
}

//Convenience function that takes care of creating each creatures FOV map and the like
Creature* create_creature(char sym, int x, int y, enum Direction direction,
                          TCOD_color_t fg, TCOD_color_t bg, short radius, Level* l) {
   
    Creature* c = malloc(sizeof(Creature));
    TCOD_map_new(SCREEN_W, SCREEN_H);

    c->sym           = sym;
    c->fg            = fg;
    c->bg            = bg;
    c->direction     = direction;
    
    c->radius        = radius;
    c->x             = x;
    c->y             = y;
    
    c->fov           = TCOD_map_new(SCREEN_W, SCREEN_H);
    c->current_level = l;

    return c;
}

void delete_creature(Creature* v) {
    TCOD_map_delete(v->fov);
    free(v);
}

/**********************
    RENDERING & FOV
**********************/

/*
    Supplied with a map from the host language. Seems the best way to do it, lots of FFI calls
    would kill performance.

    TODO: is currently hardcoded for the PC, ie expecting a player in the dead middle of the screen.
    TODO: add a switch so it can be non-directional.
*/    
static void process_fov(Creature* c, int camera_x, int camera_y, bool directional) {    
    
    int start_x;
    int end_x;
    int start_y;
    int end_y;
    
    if (directional) {
        const int MID_X = SCREEN_W / 2;
        const int MID_Y = SCREEN_H / 2;
        
        switch(c->direction) {
            case North:
                start_x = 0; 
                end_x   = SCREEN_W;
                start_y = 0;
                end_y   = MID_Y;
                break;
            
            case Northeast:           
                start_x = MID_X; 
                end_x   = SCREEN_W;
                start_y = 0;
                end_y   = MID_Y + 1;
                break;
            
            case East:
                start_x = MID_X + 1; 
                end_x   = SCREEN_W;
                start_y = 0;
                end_y   = SCREEN_H;
                break;
            
            case Southeast:
                start_x = MID_X; 
                end_x   = SCREEN_W;
                start_y = MID_Y;
                end_y   = SCREEN_H;
                break;
                
            case South:
                start_x = 0; 
                end_x   = SCREEN_W;
                start_y = MID_Y + 1;
                end_y   = SCREEN_H;
                break;
                
            case Southwest:
                start_x = 0; 
                end_x   = MID_X + 1;
                start_y = MID_Y;
                end_y   = SCREEN_H;
                break;
                
            case West:
                start_x = 0; 
                end_x   = MID_X;
                start_y = 0;
                end_y   = SCREEN_H;
                break;
                
            case Northwest:
                start_x = 0; 
                end_x   = MID_X + 1;
                start_y = 0;
                end_y   = MID_Y + 1;
                break;
            
            default:
                g_error("Creature has invalid direction");
                break;
        }
    } else {
        start_x = 0;
        end_x = SCREEN_W;
        start_y = 0;
        end_y = SCREEN_H;
    }
    
    TCOD_map_clear(c->fov);
    
    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {                       
            Tile t = get_tile(c->current_level, (x + camera_x), (y + camera_y));
            TCOD_map_set_properties(c->fov, x, y, !t.opaque, !t.solid);
        }
    }
    
    TCOD_map_compute_fov(c->fov, (SCREEN_W / 2), (SCREEN_H / 2), c->radius, true, FOV_SHADOW);
}

//I THINK "degree" should be the input from the game counter.
double day_night_cycle(long long degree) {
    double rads = (degree * G_PI) / 180.0;
    double time = 0.4 * cos(rads) + 0.6;    
       
    return time;      
}

//TODO: a list of creatures (ie the monsters on screen).
//TODO: Separate FOV from rendering here.
void render(Level* l, Coord* camera, Creature * pc, float time, bool fog_of_war, bool directional) {      
    
    assert(0.0 <= time <= 1.0);
    
    //Processing (mutating) fov before deciding what to render
    process_fov(pc, camera->x, camera->y, directional);
    
    for (int j = camera->y; j < (camera->y + SCREEN_H); j++) {
        for (int i = camera->x; i < (camera->x + SCREEN_W); i++) {
            
            Tile* t = &l->tiles[i + j * l->width];
            TCOD_color_t colour;
            
            if (!TCOD_map_is_in_fov(pc->fov, (i - camera->x), (j - camera->y))) {
                if ((!fog_of_war) || (t->seen)) {
                    colour = TCOD_color_lerp(*t->night, t->day, time);
                } else {
                    colour = TCOD_black;
                }
                
            } else {
                colour = TCOD_color_lerp(*t->night_vis, t->day_vis, time);
                t->seen = true;
            }

            TCOD_console_put_char_ex(0, i - camera->x, j - camera->y, t->sym, colour, TCOD_black); 
        }       
    }
    TCOD_console_put_char_ex(0, SCREEN_W/2, SCREEN_H/2, pc->sym, pc->fg, pc->bg); 
}

TCOD_map_t new_fov_map() {
    return TCOD_map_new(SCREEN_W, SCREEN_H);
}

/************
    TILES
************/

const TileSeed NULL_TILE_COMMON  = {0, '?', true, true, 
                                      {0,0,0}, {0,0,0}, {0,0,0}, 
                                      {0,0,0}, {0,0,0}, {0,0,0}, 
                                      0.0, 0.0};

//local helper function
static TCOD_color_t colour_mix(TCOD_color_t colour_a, TCOD_color_t colour_b, float min, float max) {
    float coefficient = TCOD_random_get_float(SEED, min, max);
    return TCOD_color_lerp(colour_a, colour_b, coefficient); 
}

//Called from outside the library
//Each time this is called it must be freed.
TileSeed* create_tile_common(char sym, 
                               bool solid, 
                               bool opaque, 
                               TCOD_color_t source_a,
                               TCOD_color_t source_b,
                               TCOD_color_t night,
                               TCOD_color_t source_a_vis,
                               TCOD_color_t source_b_vis,
                               TCOD_color_t night_vis,
                               float min,
                               float max) 
{
    //starting it at 1 because NULL_TILE is initialised at 0    
    static int uid = 1;
    TileSeed * tc = malloc(sizeof(TileSeed));
    
    if (tc != NULL) {
        tc->type         = uid;
        tc->sym          = sym;
        tc->solid        = solid;
        tc->opaque       = opaque;
        tc->source_a     = source_a;
        tc->source_b     = source_b;
        tc->night        = night,
        tc->source_a_vis = source_a_vis;
        tc->source_b_vis = source_b_vis;
        tc->night_vis    = night_vis;
        tc->min          = min;
        tc->max          = max; }
    else {
        g_error("malloc returned null when trying to allocate space for the TileSeed structure"); 
    }
        
    uid++;

    return tc; 
}

//Should not need to be used by the host, I don't think
static Tile create_tile (const TileSeed *tc) {
    TCOD_color_t day     = colour_mix(tc->source_a,     tc->source_b,     tc->min, tc->max);
    TCOD_color_t day_vis = colour_mix(tc->source_a_vis, tc->source_b_vis, tc->min, tc->max);
                                             
    const Tile tile = {
        false, 
        false, 
        
        tc->type,
        
        tc->sym,
        tc->solid,
        tc->opaque,
        
        day,
        day_vis,
        &tc->night,
        &tc->night_vis };
        
    return tile; 
}

/************
    LEVEL      
************/

//----Internal----
static guint percentage() {
    return TCOD_random_get_int(SEED, 0, 100); 
}
static void check_is_percentage(int ratio) {
    assert((ratio >= 0) && (ratio <= 100)); 
}
static bool outside_world_p(Level* l, int x, int y) {
    if (x < 0 || x >= l->width || y < 0 || y >= l->height)
        return true;
    else
        return false; 
}

//----External----

void set_tile(Level *l, guint x, guint y, const TileSeed *tc) {
    if (!outside_world_p(l, x, y)) 
        l->tiles[x + y * l->width] = create_tile(tc); 
    else
        g_warning("Attempted to set a tile outside the map at %d, %d\n", x, y);
}

Tile get_tile(Level* l, guint x, guint y) {
    if (!outside_world_p(l, x, y))    
        return l->tiles[x + y * l->width];
    else {        
        printf("Creating null tile at %d, %d\n", x, y);
        return create_tile((TileSeed*)&NULL_TILE_COMMON);
}}

Tile get_tile_relative(Level* l, guint x, guint y, Coord c) {
    //don't test the inputs, test projected OUTPUT
    return get_tile(l, x + c.x, y + c.y);        
}

//For the player to check if location can be walked on
bool walkable_p(Level* l, guint x, guint y) {
    printf("destination tile is: %d Location %d/%d\n", !get_tile(l, x, y).solid, x, y);
    return !get_tile(l, x, y).solid;
}

//checks if a location on the map has a neighbour of a certain type.
static bool is_neighbour(Level* l, TileSeed neighbour, guint x, guint y) {
    for (int i = 0; i < NUM_DIRECTIONS; i++) {
        if (get_tile_relative(l, x, y, DIRECTIONS[i]).type == neighbour.type)
            return true; 
        }    
    return false; 
}

static guint sum_of_neighbours(Level* l, TileSeed neighbour, guint x, guint y) {
    guint sum = 0;
        
    for (int i = 0; i < NUM_DIRECTIONS; i++) {
        Tile current = get_tile_relative(l, x, y, DIRECTIONS[i]);
        const int type = current.type;
        if (type == neighbour.type)
            sum++; 
    }    
    return sum; 
}

//----Generators----/
void one_tile_fill(Area* a, const TileSeed* tc) {
                       
    for (int y = a->start_y; y < a->end_y; y++) 
        for (int x = a->start_x; x < a->end_x; x++) 
            set_tile(a->level, x, y, tc); 
}

void two_tile_fill(Area* a, TileSeed* tc1, TileSeed* tc2, int ratio) {
    
    check_is_percentage(ratio);
    for (int y = a->start_y; y < a->end_y; y++) {
        for (int x = a->start_x; x < a->end_x; x++) {
            if (percentage() >= ratio)
                set_tile(a->level, x, y, tc1);
            else
                set_tile(a->level, x, y, tc2); 
        }
    }
}

void tree_pattern_fill(Area* a, TileSeed* tree1, TileSeed* tree2, int tree_number, int tree_ratio) {
    
    check_is_percentage(tree_ratio);
    
    //Dots the map with trees
    for (int i = 0; i < tree_number; i++) {
        int x_pos = TCOD_random_get_int(SEED, a->start_x, a->end_x-1);
        int y_pos = TCOD_random_get_int(SEED, a->start_y, a->end_y-1);
          
        //Ensures trees aren't placed adjacent too each other. As that's totally not how trees grow.
        if (!(is_neighbour(a->level, *tree1, x_pos, y_pos) || 
              is_neighbour(a->level, *tree2, x_pos, y_pos))) {
            
            if (percentage() >= tree_ratio)
                set_tile(a->level, x_pos, y_pos, tree1);
            else                
                set_tile(a->level, x_pos, y_pos, tree2); 
        }   
    }    
}

void veg_pattern_fill(Area* a,
                      TileSeed* veg1, 
                      TileSeed* veg2, 
                      int veg_number, 
                      int veg_ratio, 
                      TileSeed* avoid) {
                          
    //Fills map with Vegetation, where there are no tree1
    for (guint i = 0; i < veg_number; i++) {
        //TODO: I don't think I need to -1 here 
        int x_pos = TCOD_random_get_int(SEED, a->start_x, a->end_x-1);
        int y_pos = TCOD_random_get_int(SEED, a->start_y, a->end_y-1);

        if (!is_neighbour(a->level, *avoid, x_pos, y_pos)) {
            if (percentage() >= veg_ratio) {
                set_tile(a->level, x_pos, y_pos, veg1);
            } else {               
                set_tile(a->level, x_pos, y_pos, veg2); 
            }
        }
    }
}

//roguebasin.roguelikedevelopment.org/index.php?title=Cellular_Automata_Method_for_Generating_Random_Cave-Like_Levels
void cellular_automata(Area *a, TileSeed *t1, TileSeed *t2, int sum_a, int sum_b) {
    
    //Sanity checks
    assert(0 <= sum_a <= 9);
    assert(0 <= sum_b <= 9);
    
    //Breaking this seems to produce interesting results.
    //assert((sum_a + sum_b) == 9);
    
    //using true for t1, false for t2
    bool p[a->end_x][a->end_y];
    
    for (guint y = a->start_y; y < a->end_y; y++) {
        for (guint x = a->start_x; x < a->end_x; x++) {
            //if the tile is of type 1 and has 4 neighbours of type 1
            if ((get_tile(a->level, x, y).type == t1->type) && 
                 sum_of_neighbours(a->level, *t1, x, y) >= sum_a) {
                
                p[x][y] = true;
            }
            //else if the tile is not type 1 and has 5 numbers that are
            else if ((get_tile(a->level, x, y).type != t1->type) && 
                      sum_of_neighbours(a->level, *t1, x, y) >= sum_b) {
                
                p[x][y] = true;
            }
            //else it's of type 2
            else {
                p[x][y] = false;
            }
        }
    }
        
    //Applying the results of the above computation, as they have to be done simultaneously.
    for (guint y = a->start_y; y < a->end_y; y++) {
        for (guint x = a->start_x; x < a->end_x; x++) {
            if (p[x][y])
                set_tile(a->level, x, y, t1);
            else
                set_tile(a->level, x, y, t2); 
        }
    }
}

static bool my_callback(TCOD_bsp_t *node, void *userData) {   
    guint start_x = node->x + TCOD_random_get_int(SEED, 1, 4);
    guint start_y = node->y + TCOD_random_get_int(SEED, 1, 4);
    guint end_x   = node->w - TCOD_random_get_int(SEED, 1, 4);
    guint end_y   = node->h - TCOD_random_get_int(SEED, 1, 4);
    
    //If the node is a leaf, fill it in
    if ((TCOD_bsp_left(node) == NULL) && (TCOD_bsp_right(node) == NULL)) {
        for (guint y = start_y; y < end_y; y++) {
            for (guint x = start_x; x < end_x; x++) {
                set_tile(userData, x, y, &NULL_TILE_COMMON);
            }
        }       
        return true;
    } else {        
        return false;
    }   
}

void rectangular_dungeon_fill(Level* l, 
                              guint start_x,    
                              guint start_y,    
                              guint end_x,  
                              guint end_y) {
    
    //Creating a BSP tree.
    TCOD_bsp_t* root = TCOD_bsp_new_with_size(start_x, start_y, end_x, end_y);
    
    //Splits the root node 5 times
    TCOD_bsp_split_recursive(root, NULL, 10, 2, 2, 1.0f, 1.0f);    
    
    TCOD_bsp_traverse_post_order(root, my_callback, l);    
}

/***********
    LEVEL
************/
Level* create_level(int width, int height) {
    Level* l = malloc(sizeof(Level));
    l->tiles = malloc(width * height * (sizeof(Tile)));
    l->width = width;
    l->height = height;
    
    //Fill map with dummy tiles..safer than dealing with "real" null tiles    
    one_tile_fill(&(Area){l, 0, 0, width, height}, &NULL_TILE_COMMON);
    
    return l;
}

void delete_level(Level* l) {
    free(l->tiles);
    free(l);
}

/**********
    BSP
**********/

static bool is_leaf(BSP_node* n) {
    if ((n->left == NULL) && (n->right == NULL)) {
        return true;
    } else {
        return false;
    }    
}

BSP_node* create_bsp_node(Area* a, struct BSP_node* left, struct BSP_node* right) {    
    BSP_node* b = malloc(sizeof(BSP_node));
    
    b->start_x = a->start_x;
    b->start_y = a->start_y;
    b->end_x   = a->end_x;
    b->end_y   = a->end_y;
    b->left    = left;
    b->right   = right;
    
    return b;
}

void create_bsp_tree(BSP_node* parent, 
                     const guint min_width, 
                     const guint min_height, 
                     GList** leaves) {
    
    static guint iterations = 0;
    iterations++;
    
    if (iterations > 50) {
        g_error("Recursion too deep: terminated");
    }    
    
    //TODO: delete this, hard coded values, for testing only.
    assert(0 <= parent->start_x <= 55);
    assert(0 <= parent->start_y <= 55);
    assert(0 <= parent->end_x <= 55);
    assert(0 <= parent->end_y <= 55);
    
    //these values bound the location of the splitting hyperplane.                           
    int min_x = parent->start_x + min_width;
    int max_x = parent->end_x   - min_width;
    int min_y = parent->start_y + min_height;
    int max_y = parent->end_y   - min_height;
    
    printf("min_x: %d | ", min_x);
    printf("max_x: %d | ", max_x);
    printf("min_y: %d | ", min_y);
    printf("max_y: %d\n",  max_y);
    
    bool a = max_x <= min_x;
    bool b = max_y <= min_y;
    bool c = (parent->end_x - parent->start_x) <= min_width;
    bool d = (parent->end_y - parent->start_y) <= min_height;
    
    //Room is too small so exit function
    if (a || b || c || d) {
        printf("room is too small - recursion exited\n");        
        *leaves = g_list_append(*leaves, parent);
        return;
    } 
    //Construct the tree
    else {    
        //Random
        bool is_horizontal = TCOD_random_get_int(SEED, 0, 1);       
        guint hyperplane;         
        BSP_node* left;
        BSP_node* right;
        Area left_area;
        Area right_area;
        
        //Horizontal split
        if (is_horizontal) {           
            printf("Horizontal split\n");
            hyperplane = TCOD_random_get_int(SEED, min_y, max_y);
            
            left_area = (Area){parent->level, 
                         parent->start_x, 
                         parent->start_y, 
                         parent->end_x - hyperplane, 
                         parent->end_y};            
            
            left = create_bsp_node(&left_area, NULL, NULL);            
            
            right_area = (Area){parent->level,
                          parent->start_x + hyperplane,
                          parent->start_y,
                          parent->end_x,
                          parent->end_y};
            
            right = create_bsp_node(&right_area, NULL, NULL);
        } 
        //Vertical split
        else {           
            printf("Vertical split\n");
            hyperplane = TCOD_random_get_int(SEED, min_x, max_x);
            
            left_area = (Area){parent->level,
                          parent->start_x, 
                          parent->start_y,            
                          parent->end_x,
                          parent->end_y - hyperplane};                                   
            
            left = create_bsp_node(&left_area, NULL, NULL);             
            
            right_area = (Area){parent->level, 
                           parent->start_x,
                           parent->start_y + hyperplane,
                           parent->end_x,
                           parent->end_y};
            
            right = create_bsp_node(&right_area, NULL, NULL);
        }
        
        printf("Hyperplane at %d\n", hyperplane);
        
        //Assigning newly created nodes to the parent node.
        parent->left  = (struct BSP_node*)left;
        parent->right = (struct BSP_node*)right;        
        
        //Repeating the process with the left and right nodes
        create_bsp_tree(left,  min_width, min_height, leaves);
        create_bsp_tree(right, min_width, min_height, leaves);              
    }
}

void carve_rectangular_room (gpointer element_data, gpointer user_data) {   
    BSP_node* node = (BSP_node*) element_data;
    TileSeed* tc = (TileSeed*) user_data;
    
    //Bug checking
    static int called = 0;
    called++;
    printf("Called %d times\n", called);    
    
    const int max_offset = 2;
    
    int start_x = node->start_x + TCOD_random_get_int(SEED, 0, max_offset);
    int start_y = node->start_y + TCOD_random_get_int(SEED, 0, max_offset);
    int end_x   = node->end_x   - TCOD_random_get_int(SEED, 0, max_offset);
    int end_y   = node->end_y   - TCOD_random_get_int(SEED, 0, max_offset);    
    
    int i = 0;
    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {            
            //Bug checking
            printf("(%d, %d)\n", x, y);
            i++;
            if (i > 1000) {
                g_error("Looping too many times");
            }
            
            //This is the function causing the segfault - but it only calls it sometimes.
            set_tile(node->level, x, y, (const TileSeed *)tc);
        }
    }
    printf("\n");    
}
