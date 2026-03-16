/*
 * ============================================
 *  CATUR JAWA untuk Game Boy Original (DMG)
 *  Dibuat dengan GBDK-2020
 *  AI menggunakan algoritma Minimax
 * ============================================
 *
 *  ATURAN:
 *  - Board 3x3 dengan 9 titik (node)
 *  - P1 (PUTIH) mulai di baris bawah (6,7,8)
 *  - P2/AI (HITAM) mulai di baris atas (0,1,2)
 *  - Bidak bergerak ke titik tetangga kosong
 *  - Menang: susun 3 bidak sejajar (tapi bukan baris awal)
 *  - Blokade: lawan tak bisa gerak = menang
 *
 *  KONTROL:
 *  - D-Pad: pilih node
 *  - A: pilih bidak / konfirmasi gerak
 *  - B: batalkan pilihan
 *  - START: reset game
 */

#include <gb/gb.h>
#include <gb/drawing.h>
#include <stdint.h>

/* ========================
   KONSTANTA & DEFINISI
   ======================== */

#define EMPTY  0
#define PLAYER 1   /* Putih - manusia */
#define AI     2   /* Hitam - komputer */

#define BOARD_SIZE 9

/* Posisi piksel untuk setiap node di layar (160x144) */
/* Node layout:
   0 - 1 - 2
   |\ | /|
   | \|/ |
   3 - 4 - 5
   | /|\ |
   |/ | \|
   6 - 7 - 8
*/

/* Koordinat X,Y setiap node dalam tile unit (8px per tile) */
/* Layar GB: 160x144px = 20x18 tile */
/* Board ditempatkan di tengah */

#define NODE_START_X  32   /* pixel x node pertama */
#define NODE_START_Y  20   /* pixel y node pertama */
#define NODE_GAP      40   /* jarak antar node dalam pixel */

/* Koordinat tiap node (pixel) */
const uint8_t node_x[9] = {
    NODE_START_X,              NODE_START_X + NODE_GAP,     NODE_START_X + NODE_GAP*2,
    NODE_START_X,              NODE_START_X + NODE_GAP,     NODE_START_X + NODE_GAP*2,
    NODE_START_X,              NODE_START_X + NODE_GAP,     NODE_START_X + NODE_GAP*2
};

const uint8_t node_y[9] = {
    NODE_START_Y,              NODE_START_Y,                NODE_START_Y,
    NODE_START_Y + NODE_GAP,   NODE_START_Y + NODE_GAP,     NODE_START_Y + NODE_GAP,
    NODE_START_Y + NODE_GAP*2, NODE_START_Y + NODE_GAP*2,  NODE_START_Y + NODE_GAP*2
};

/* Adjacency list setiap node */
/* -1 = akhir list */
const int8_t adj[9][9] = {
    {1, 3, 4, -1},           /* node 0 */
    {0, 2, 4, -1},           /* node 1 */
    {1, 4, 5, -1},           /* node 2 */
    {0, 4, 6, -1},           /* node 3 */
    {0, 1, 2, 3, 5, 6, 7, 8, -1}, /* node 4 (tengah - terhubung semua) */
    {2, 4, 8, -1},           /* node 5 */
    {3, 4, 7, -1},           /* node 6 */
    {6, 4, 8, -1},           /* node 7 */
    {5, 4, 7, -1}            /* node 8 */
};

/* Pola menang: 3 node sejajar */
const uint8_t win_patterns[8][3] = {
    {0, 1, 2},  /* baris atas */
    {3, 4, 5},  /* baris tengah */
    {6, 7, 8},  /* baris bawah */
    {0, 3, 6},  /* kolom kiri */
    {1, 4, 7},  /* kolom tengah */
    {2, 5, 8},  /* kolom kanan */
    {0, 4, 8},  /* diagonal \ */
    {2, 4, 6}   /* diagonal / */
};

/* ========================
   VARIABEL GLOBAL
   ======================== */

uint8_t board[BOARD_SIZE];       /* state papan */
uint8_t current_player;          /* giliran siapa */
int8_t  selected_node;           /* node yang dipilih (-1 = belum) */
uint8_t cursor_node;             /* posisi kursor */
uint8_t game_active;             /* 1=main, 0=selesai */
uint8_t ai_thinking;             /* flag: AI sedang berpikir */

/* ========================
   FUNGSI HELPER BOARD
   ======================== */

/* Cek apakah dua node bertetangga */
uint8_t is_adjacent(uint8_t from, uint8_t to) {
    uint8_t i = 0;
    while (adj[from][i] != -1) {
        if (adj[from][i] == to) return 1;
        i++;
    }
    return 0;
}

/* Cek apakah player punya langkah legal */
uint8_t has_moves(uint8_t player) {
    uint8_t i, j;
    for (i = 0; i < BOARD_SIZE; i++) {
        if (board[i] == player) {
            j = 0;
            while (adj[i][j] != -1) {
                if (board[adj[i][j]] == EMPTY) return 1;
                j++;
            }
        }
    }
    return 0;
}

/* Cek kemenangan - return 1 jika player menang */
uint8_t check_win(uint8_t player) {
    uint8_t p, a, b, c;
    uint8_t initial_row_a, initial_row_b, initial_row_c;

    for (p = 0; p < 8; p++) {
        a = win_patterns[p][0];
        b = win_patterns[p][1];
        c = win_patterns[p][2];

        if (board[a] == player && board[b] == player && board[c] == player) {
            /* Cek bukan baris awal sendiri */
            if (player == PLAYER) {
                /* Baris awal PLAYER adalah 6,7,8 */
                if (a == 6 && b == 7 && c == 8) continue;
            } else {
                /* Baris awal AI adalah 0,1,2 */
                if (a == 0 && b == 1 && c == 2) continue;
            }
            return 1;
        }
    }
    return 0;
}

/* Hitung jumlah bidak player */
uint8_t count_pieces(uint8_t player) {
    uint8_t i, count = 0;
    for (i = 0; i < BOARD_SIZE; i++) {
        if (board[i] == player) count++;
    }
    return count;
}

/* ========================
   MINIMAX AI
   ======================== */

/* Evaluasi papan untuk AI */
int8_t evaluate(void) {
    if (check_win(AI))     return  10;
    if (check_win(PLAYER)) return -10;
    if (!has_moves(AI))    return -10;  /* AI terjebak = kalah */
    if (!has_moves(PLAYER)) return 10;  /* Player terjebak = AI menang */
    return 0;
}

/* Minimax dengan depth terbatas (depth=3 untuk kecepatan GB) */
int8_t minimax(uint8_t depth, uint8_t is_maximizing) {
    int8_t score = evaluate();

    /* Terminal state */
    if (score != 0) return score;
    if (depth == 0) return 0;

    uint8_t curr = is_maximizing ? AI : PLAYER;
    int8_t best, val;
    uint8_t from, j, to;

    if (is_maximizing) {
        best = -127;
        for (from = 0; from < BOARD_SIZE; from++) {
            if (board[from] == AI) {
                j = 0;
                while (adj[from][j] != -1) {
                    to = adj[from][j];
                    if (board[to] == EMPTY) {
                        /* Lakukan langkah */
                        board[to] = AI;
                        board[from] = EMPTY;

                        val = minimax(depth - 1, 0);
                        if (val > best) best = val;

                        /* Undo langkah */
                        board[from] = AI;
                        board[to] = EMPTY;
                    }
                    j++;
                }
            }
        }
        return best;
    } else {
        best = 127;
        for (from = 0; from < BOARD_SIZE; from++) {
            if (board[from] == PLAYER) {
                j = 0;
                while (adj[from][j] != -1) {
                    to = adj[from][j];
                    if (board[to] == EMPTY) {
                        board[to] = PLAYER;
                        board[from] = EMPTY;

                        val = minimax(depth - 1, 1);
                        if (val < best) best = val;

                        board[from] = PLAYER;
                        board[to] = EMPTY;
                    }
                    j++;
                }
            }
        }
        return best;
    }
}

/* AI cari langkah terbaik dan lakukan */
void ai_move(void) {
    int8_t best_score = -127;
    int8_t score;
    uint8_t best_from = 0, best_to = 0;
    uint8_t from, j, to;
    uint8_t found = 0;

    for (from = 0; from < BOARD_SIZE; from++) {
        if (board[from] == AI) {
            j = 0;
            while (adj[from][j] != -1) {
                to = adj[from][j];
                if (board[to] == EMPTY) {
                    board[to] = AI;
                    board[from] = EMPTY;

                    score = minimax(3, 0);  /* depth=3 */

                    board[from] = AI;
                    board[to] = EMPTY;

                    if (score > best_score || !found) {
                        best_score = score;
                        best_from = from;
                        best_to = to;
                        found = 1;
                    }
                }
                j++;
            }
        }
    }

    if (found) {
        board[best_to] = AI;
        board[best_from] = EMPTY;
    }
}

/* ========================
   FUNGSI GAMBAR (DRAWING)
   ======================== */

void draw_board(void) {
    uint8_t i;

    /* Hapus layar */
    cls();

    /* === Gambar garis koneksi antar node === */
    /* Horizontal */
    /* Baris 0: 0-1-2 */
    line(node_x[0], node_y[0], node_x[1], node_y[1]);
    line(node_x[1], node_y[1], node_x[2], node_y[2]);
    /* Baris 1: 3-4-5 */
    line(node_x[3], node_y[3], node_x[4], node_y[4]);
    line(node_x[4], node_y[4], node_x[5], node_y[5]);
    /* Baris 2: 6-7-8 */
    line(node_x[6], node_y[6], node_x[7], node_y[7]);
    line(node_x[7], node_y[7], node_x[8], node_y[8]);

    /* Vertical */
    line(node_x[0], node_y[0], node_x[3], node_y[3]);
    line(node_x[3], node_y[3], node_x[6], node_y[6]);
    line(node_x[1], node_y[1], node_x[4], node_y[4]);
    line(node_x[4], node_y[4], node_x[7], node_y[7]);
    line(node_x[2], node_y[2], node_x[5], node_y[5]);
    line(node_x[5], node_y[5], node_x[8], node_y[8]);

    /* Diagonal */
    line(node_x[0], node_y[0], node_x[4], node_y[4]);
    line(node_x[4], node_y[4], node_x[8], node_y[8]);
    line(node_x[2], node_y[2], node_x[4], node_y[4]);
    line(node_x[4], node_y[4], node_x[6], node_y[6]);

    /* === Gambar node (titik) dan bidak === */
    for (i = 0; i < BOARD_SIZE; i++) {
        uint8_t x = node_x[i];
        uint8_t y = node_y[i];

        if (board[i] == PLAYER) {
            /* Bidak putih: lingkaran kosong besar */
            circle(x, y, 6, SOLID);
            /* Isi putih dengan invert = lingkaran luar hitam, dalam putih */
            box(x-4, y-4, x+4, y+4, SOLID);
            circle(x, y, 5, WHITE);  /* lingkaran putih */
        } else if (board[i] == AI) {
            /* Bidak hitam: lingkaran penuh */
            circle(x, y, 6, SOLID);
        } else {
            /* Node kosong: titik kecil */
            circle(x, y, 2, SOLID);
        }

        /* Highlight kursor */
        if (i == cursor_node) {
            /* Kotak kursor di sekitar node */
            box(x-8, y-8, x+8, y+8, M_NOFILL);
        }

        /* Highlight node terpilih (bidak yang akan dipindah) */
        if (i == (uint8_t)selected_node && selected_node != -1) {
            circle(x, y, 9, M_NOFILL);
        }
    }

    /* Highlight langkah valid jika bidak sudah dipilih */
    if (selected_node != -1 && current_player == PLAYER) {
        uint8_t j2 = 0;
        while (adj[selected_node][j2] != -1) {
            uint8_t target = adj[selected_node][j2];
            if (board[target] == EMPTY) {
                uint8_t tx = node_x[target];
                uint8_t ty = node_y[target];
                /* Tanda X untuk langkah valid */
                line(tx-4, ty-4, tx+4, ty+4);
                line(tx+4, ty-4, tx-4, ty+4);
            }
            j2++;
        }
    }

    /* === Status teks di bawah === */
    gotoxy(0, 16);
    if (!game_active) {
        if (check_win(PLAYER)) {
            print("  KAMU MENANG!  ");
        } else if (check_win(AI)) {
            print("  AI MENANG!    ");
        } else {
            print("  DRAW!         ");
        }
        gotoxy(0, 17);
        print("  START=ULANG   ");
    } else if (ai_thinking) {
        print("  AI berpikir.. ");
    } else if (current_player == PLAYER) {
        if (selected_node == -1) {
            print("  Pilih bidak(A)");
        } else {
            print("  Pilih tujuan  ");
        }
    }
}

/* ========================
   INIT GAME
   ======================== */

void init_game(void) {
    uint8_t i;

    /* Setup board awal */
    for (i = 0; i < BOARD_SIZE; i++) board[i] = EMPTY;
    board[0] = AI;     board[1] = AI;     board[2] = AI;
    board[6] = PLAYER; board[7] = PLAYER; board[8] = PLAYER;

    current_player = PLAYER;
    selected_node  = -1;
    cursor_node    = 7;   /* mulai di tengah bawah */
    game_active    = 1;
    ai_thinking    = 0;
}

/* ========================
   NAVIGASI KURSOR
   ======================== */

/* Pindah kursor ke node tetangga terdekat sesuai arah */
void move_cursor(uint8_t direction) {
    /* direction: 0=up, 1=down, 2=left, 3=right */
    uint8_t row = cursor_node / 3;
    uint8_t col = cursor_node % 3;

    if (direction == 0 && row > 0) cursor_node -= 3;       /* up */
    if (direction == 1 && row < 2) cursor_node += 3;       /* down */
    if (direction == 2 && col > 0) cursor_node -= 1;       /* left */
    if (direction == 3 && col < 2) cursor_node += 1;       /* right */
}

/* ========================
   MAIN LOOP
   ======================== */

void main(void) {
    uint8_t keys;
    uint8_t prev_keys = 0;

    /* Init GBDK */
    DISPLAY_ON;

    init_game();
    draw_board();

    while (1) {
        /* Tunggu VBlank untuk input yang smooth */
        wait_vbl_done();

        keys = joypad();
        uint8_t pressed = keys & ~prev_keys;  /* only newly pressed */
        prev_keys = keys;

        /* === Input saat giliran player === */
        if (game_active && current_player == PLAYER && !ai_thinking) {

            /* Navigasi kursor */
            if (pressed & J_UP)    move_cursor(0);
            if (pressed & J_DOWN)  move_cursor(1);
            if (pressed & J_LEFT)  move_cursor(2);
            if (pressed & J_RIGHT) move_cursor(3);

            /* Tombol A: pilih / gerak */
            if (pressed & J_A) {
                if (selected_node == -1) {
                    /* Pilih bidak player */
                    if (board[cursor_node] == PLAYER) {
                        selected_node = cursor_node;
                    }
                } else {
                    /* Gerakkan ke node target */
                    if (board[cursor_node] == EMPTY &&
                        is_adjacent((uint8_t)selected_node, cursor_node)) {

                        board[cursor_node] = PLAYER;
                        board[selected_node] = EMPTY;
                        selected_node = -1;

                        /* Cek menang */
                        if (check_win(PLAYER)) {
                            game_active = 0;
                        } else if (!has_moves(AI)) {
                            game_active = 0;  /* AI blokade */
                        } else {
                            current_player = AI;
                            ai_thinking = 1;
                        }
                    } else if (board[cursor_node] == PLAYER) {
                        /* Ganti pilihan ke bidak lain */
                        selected_node = cursor_node;
                    } else {
                        /* Batalkan jika klik tempat invalid */
                        selected_node = -1;
                    }
                }
            }

            /* Tombol B: batal pilihan */
            if (pressed & J_B) {
                selected_node = -1;
            }
        }

        /* === Giliran AI === */
        if (game_active && current_player == AI) {
            if (ai_thinking) {
                draw_board();  /* tampilkan "AI berpikir.." */
                ai_thinking = 0;

                /* AI berpikir */
                ai_move();

                /* Cek menang */
                if (check_win(AI)) {
                    game_active = 0;
                } else if (!has_moves(PLAYER)) {
                    game_active = 0;  /* Player blokade */
                } else {
                    current_player = PLAYER;
                }
            }
        }

        /* === Reset dengan START === */
        if (pressed & J_START) {
            init_game();
        }

        /* Gambar ulang layar */
        draw_board();
    }
}
