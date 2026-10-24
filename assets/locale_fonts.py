# Pixel fonts for localising zelda3's interface: words drawn in the style of the original graphics, so a translation
# can relabel the HUD, the item menu and the title screen. Each glyph is a list of rows; 'X' is a letter pixel.
# The designs are this project's own; letters are added as translations need them (a missing one is reported).

# The 3x5 capitals of the item menu's headings (PENDANTS, EQUIPMENT, ...): letter colour 2 on the box colour 3.
# Rows 0..6 of a tile: the letter takes rows 1-5; an accent goes on row 0 and a cedilla on row 6.
SMALL = {
  'A': ['.X.', 'X.X', 'XXX', 'X.X', 'X.X'],
  'B': ['XX.', 'X.X', 'XX.', 'X.X', 'XX.'],
  'C': ['.XX', 'X..', 'X..', 'X..', '.XX'],
  'D': ['XX.', 'X.X', 'X.X', 'X.X', 'XX.'],
  'E': ['XXX', 'X..', 'XX.', 'X..', 'XXX'],
  'F': ['XXX', 'X..', 'XX.', 'X..', 'X..'],
  'G': ['.XX', 'X..', 'X.X', 'X.X', '.XX'],
  'H': ['X.X', 'X.X', 'XXX', 'X.X', 'X.X'],
  'I': ['X', 'X', 'X', 'X', 'X'],
  'J': ['..X', '..X', '..X', 'X.X', '.X.'],
  'K': ['X.X', 'X.X', 'XX.', 'X.X', 'X.X'],
  'L': ['X..', 'X..', 'X..', 'X..', 'XXX'],
  'M': ['X...X', 'XX.XX', 'X.X.X', 'X...X', 'X...X'],
  'N': ['X..X', 'XX.X', 'X.XX', 'X..X', 'X..X'],
  'O': ['.X.', 'X.X', 'X.X', 'X.X', '.X.'],
  'P': ['XX.', 'X.X', 'XX.', 'X..', 'X..'],
  'Q': ['.X.', 'X.X', 'X.X', 'XX.', '.XX'],
  'R': ['XX.', 'X.X', 'XX.', 'X.X', 'X.X'],
  'S': ['.XX', 'X..', '.X.', '..X', 'XX.'],
  'T': ['XXX', '.X.', '.X.', '.X.', '.X.'],
  'U': ['X.X', 'X.X', 'X.X', 'X.X', '.X.'],
  'V': ['X.X', 'X.X', 'X.X', '.X.', '.X.'],
  'W': ['X...X', 'X...X', 'X.X.X', 'XX.XX', 'X...X'],
  'X': ['X.X', 'X.X', '.X.', 'X.X', 'X.X'],
  'Y': ['X.X', 'X.X', '.X.', '.X.', '.X.'],
  'Z': ['XXX', '..X', '.X.', 'X..', 'XXX'],
  '.': ['.', '.', '.', '.', 'X'],
  '-': ['..', '..', 'XX', '..', '..'],
}
# Marks for the row above (acute, circumflex, tilde) and below (cedilla), centred on a 3-wide letter.
SMALL_ACCENTS = {'´': '..X', '^': '.X.', '~': 'XX.', ',': '.X.'}
DECOMPOSE = {'Á': 'A´', 'É': 'E´', 'Í': 'I´', 'Ó': 'O´', 'Ú': 'U´', 'Â': 'A^', 'Ê': 'E^', 'Ô': 'O^',
             'Ã': 'A~', 'Õ': 'O~', 'Ç': 'C,', 'À': 'A`'}

# The HUD's "LIFE" lettering: colour 2 with a colour-3 shadow one pixel right and below, on transparent pixels.
HUD = {
  'A': ['.X.', 'X.X', 'X.X', 'XXX', 'X.X', 'X.X'],
  'D': ['XX.', 'X.X', 'X.X', 'X.X', 'X.X', 'XX.'],
  'E': ['XXX', 'X..', 'XXX', 'X..', 'X..', 'XXX'],
  'F': ['XXX', 'X..', 'XXX', 'X..', 'X..', 'X..'],
  'I': ['X', 'X', 'X', 'X', 'X', 'X'],
  'L': ['X..', 'X..', 'X..', 'X..', 'X..', 'XXX'],
  'V': ['X.X', 'X.X', 'X.X', 'X.X', 'X.X', '.X.'],
}

# The title screen's serif capitals (the line under ZELDA): 11 rows, outlined and shaded by the builder.
TITLE = {
  'A': ['...XX...', '...XX...', '..X.XX..', '..X.XX..', '..X.XX..', '.X...XX.', '.XXXXXX.', '.X...XX.',
        'X.....XX', 'X.....XX', 'XX...XXX'],
  'C': ['..XXXX.X', '.XX...XX', 'XX.....X', 'XX......', 'XX......', 'XX......', 'XX......', 'XX......',
        'XX.....X', '.XX...X.', '..XXXX..'],
  'D': ['XXXXXX..', '.XX..XX.', '.XX...XX', '.XX...XX', '.XX...XX', '.XX...XX', '.XX...XX', '.XX...XX',
        '.XX...XX', '.XX..XX.', 'XXXXXX..'],
  'E': ['XXXXXXX', '.XX...X', '.XX....', '.XX..X.', '.XXXXX.', '.XX..X.', '.XX....', '.XX....', '.XX....',
        '.XX...X', 'XXXXXXX'],
  'L': ['XXXX...', '.XX....', '.XX....', '.XX....', '.XX....', '.XX....', '.XX....', '.XX....', '.XX....',
        '.XX...X', 'XXXXXXX'],
  'M': ['XXX....XXX', '.XX....XX.', '.XXX..XXX.', '.X.X..X.X.', '.X.XX.X.X.', '.X..XX..X.', '.X..XX..X.',
        '.X......X.', '.X......X.', '.X......X.', 'XXX....XXX'],
  'O': ['..XXXX..', '.XX..XX.', 'XX....XX', 'XX....XX', 'XX....XX', 'XX....XX', 'XX....XX', 'XX....XX',
        'XX....XX', '.XX..XX.', '..XXXX..'],
  'P': ['XXXXXX..', '.XX..XX.', '.XX...XX', '.XX...XX', '.XX..XX.', '.XXXXX..', '.XX.....', '.XX.....',
        '.XX.....', '.XX.....', 'XXXX....'],
  'S': ['.XXXX.X', 'XX...XX', 'XX....X', 'XXX....', '.XXX...', '..XXX..', '...XXX.', '....XXX', 'X....XX',
        'XX...XX', 'X.XXXX.'],
  'U': ['XXXX.XXX', '.XX...X.', '.XX...X.', '.XX...X.', '.XX...X.', '.XX...X.', '.XX...X.', '.XX...X.',
        '.XX...X.', '.XX..XX.', '..XXXX..'],
}
TITLE_SPACE = 4
