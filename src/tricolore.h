// Sprite registers (not to be confused with sprite data memory)
#define SPRITE_REGS 0x0100
// Palette
#define PALETTE_REGS 0x00C0

void tricolore_init();

extern Variable * direction;
extern Variable * pointer_x;
extern Variable * pointer_y;
extern Variable * click;
extern Variable * hires;

extern bool screen_active;

// We assign the horizontal and vertical planes both their own 2-bit groups,
// so that they can be activated in combination.
// As for left <-> right / up <-> down, we could assign each their own bit as well,
// but alternatively we can say bit 1 = active plane; bit 2 = direction.
// This way, we can derive a signed direction using number - 2.
#define DIR_LEFT   0b0100
#define DIR_RIGHT  0b1100
#define DIR_UP     0b0001
#define DIR_DOWN   0b0011
#define PLANE_HOR  0b1100
#define PLANE_VERT 0b0011
#define PLANE_BTN 0b110000


// TODO: thought we already were cramped for space here, but we only use 10 of the 16 bytes available!
typedef struct Sprite {
  uint8_t x;
  uint8_t y;
  uint8_t width;
  uint8_t height;
  uint16_t map; // location of map in memory. Must be 16 bit, otherwise granularity is way off
  uint8_t mode; // 6-bit (MSB) location of tiles (presently offset from 24k = 0x6000) + 2-bit mode
  uint8_t flags; // msb->lsb: scaley (2b) scalex (2b), transparent (4b);
  uint16_t colors; // 4x index into 4-bit color palette
} Sprite;

// Extend start-of / top-of register
enum {
    TILES=5,
    SCREEN
};

void draw(int from_x, int from_y, int width, int height);

// These methods must be implemented by a screen library
void set_pixel(int x, int y, uint32_t color);
void redraw(int from_x, int from_y, int width, int height);

// And these properties
extern int SCREEN_WIDTH; // retained caps from earlier define
extern int SCREEN_HEIGHT;

// Non-screen functions expected to exist on the Tricolore platform as well
void beep(int frequency, int duration);

//
// Callbacks for which Pasta expects an implementation
//
bool update_inputs(); // updates button state; returns whether there's input on stdin (halts catchup loops)

