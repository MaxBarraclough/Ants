/*
 * Created by Max Barraclough, 2013.
 *
 * Based loosely on the structure of the code of
 * the lessons at https://github.com/MaxBarraclough/NeHe-SFML2
 *
 * Released under the GNU General Public Licence version 2.
 */

#if ( defined WIN32 || defined __WIN32 || defined __WIN32__ )
    #define _CRT_RAND_S // In Windows, this enables the rand_s function
    #define alloca _alloca
#endif

#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

// #include <random>
#include <stdlib.h> // for rand(), or in Windows, rand_s
#include <time.h> // for time(NULL)
#include <vector>



/* we seem to be ok without this for fputs and stderr, but it should really be included: */
// #include "stdio.h"

/* Static constants */

static const char * const WINDOW_TITLE = "Ants in OpenGL";

static const double IMAGE_WIDTH_TO_GRID_WIDTH_MULTIPLIER = 1.5;
static const double IMAGE_HEIGHT_TO_GRID_HEIGHT_MULTIPLIER = IMAGE_WIDTH_TO_GRID_WIDTH_MULTIPLIER;

static const unsigned int FIXED_WINDOW_WIDTH  = 640;
static const unsigned int FIXED_WINDOW_HEIGHT = 640;

static const unsigned int NUM_MOVES_PER_KEYPRESS = 226;

/* Statics */

static unsigned int number_of_ants;

static bool fullscreen = false;
static bool vsync      = true;

static unsigned int grid_width;
static unsigned int grid_height;

static bool *grid; // Really a 2d array on [x][y]. Will be initialised to all false.      true -> white   false -> black


void initImage(const char inputImageFile[]) {
// grid[0][0] = true; grid[grid_width-10][grid_height-10] = true;
        sf::Image image;
//      if (image.loadFromFile("/Users/mb/Documents/MyNeheTuts/ants/in.bmp")) {
        if ( image.loadFromFile(inputImageFile) ) {
            const sf::Vector2u& size = image.getSize(); ///// good practice to use the magic & here?
            const unsigned int imageWidth  = size.x;
            const unsigned int imageHeight = size.y;

            grid_width  = imageWidth  * IMAGE_WIDTH_TO_GRID_WIDTH_MULTIPLIER;
            grid_height = imageHeight * IMAGE_HEIGHT_TO_GRID_HEIGHT_MULTIPLIER;

            // the () causes value-initialisation (i.e. false), see http://stackoverflow.com/a/2204380
            grid = new bool[grid_width * grid_height]();

            const int verticalOffset   = ( grid_height - imageHeight ) / 2 ;
            const int horizontalOffset = ( grid_width  - imageWidth  ) / 2 ;

            const sf::Uint8 * const pixelsPtr = image.getPixelsPtr();

            // In this loop we iterate through the bitmap array
            // and set to 'true' the appropriate elements in our 'grid' array
            for (unsigned int currentRow = 0; currentRow != imageHeight; ++currentRow) {
//          for (unsigned int currentRow = imageHeight - 1; currentRow >= 0; --currentRow) { // TODO why does this not work?

                const sf::Uint8 * const basisPixelPointerForCurrentRow = pixelsPtr + (4 * currentRow * imageWidth);

                // our array is [x][y] so some weirdness occurs here to get the per-row pointer to be possible
                bool * const basisGridPointerForCurrentRow = ((bool*)grid) + currentRow + verticalOffset; // notice zero offset in the major

                for (unsigned int currentColumn = 0; currentColumn != imageWidth; ++currentColumn) { // TODO maybe do these loops backwards
//              for (unsigned int currentColumn = imageWidth - 1; currentColumn >= 0; --currentColumn) { // TODO why does this not work???
                    basisGridPointerForCurrentRow[(currentColumn + horizontalOffset) * grid_height] =
                      ( basisPixelPointerForCurrentRow[currentColumn * 4] == 0 );
                }
            }
        }
        else {
            fputs("ERROR: File not found\n", stderr);
            exit(3);
        }
}


enum Direction_t {UP = 0, RIGHT = 1, DOWN = 2, LEFT = 3};


static const int deltaMap[] = {0, 1, 0, -1, 0};

inline int xDeltaForDirection(Direction_t d) {
//      static const int map[] = {0, 1, 0, -1};

        return deltaMap[d];
}

inline int yDeltaForDirection(Direction_t d) {
//      static const int map[] = {1, 0, -1, 0};
        static const int * const map = deltaMap + 1;

        return map[d];
}

inline Direction_t rotateRightBy(Direction_t d, const unsigned int thisManyRightAngles) {
        return (Direction_t) ((d + thisManyRightAngles) & 3); // &3 gives us the effect of %4
}

//  // rotate right (clockwise, from our top-down perspective)
//  inline Direction_t rotateRight(Direction_t d) {
//          return rotateRightBy(d, 1);
//  }
//
//  // rotate left (anti-clockwise, from our top-down perspective)
//  inline Direction_t rotateLeft(Direction_t d) {
//          return rotateRightBy(d, 3);
//  }


class Ant {
        Direction_t direction;

      public:
        int x, y; // avoiding 'unsigned', just so we better match OpenGL when the time comes
        // Ant(Ant &toClone) {}

        Ant(int xInit, int yInit) : x(xInit), y(yInit), direction(UP) { }

        // ~Ant() { }

        // return true if the ant moved out-of-bounds
        bool rotateAndMove() {
                const unsigned int index = (this->x * grid_height) + this->y;

                // Invert the bool of the appropriate index. The new value decides which direction our ant turns.

                // if (grid[index] = !grid[index]) then go right by passing in 1, else, pass in 3 to go left.
                this->direction = rotateRightBy( this->direction,   ( (grid[index] = !grid[index]) ) ? 1 : 3   );

                bool toReturn = false;

                const int newX = this->x + xDeltaForDirection(this->direction);
                const int newY = this->y + yDeltaForDirection(this->direction);

                if (newX >= grid_width || newX < 0 || newY >= grid_height || newY < 0) {
                    toReturn = true;
                } else {
                    this->x = newX;
                    this->y = newY;
                }
                return toReturn;
        }

        void draw() {
                glColor3ub((GLubyte)(0.7 * 255.0), 0, 0);

                const int inverted_y = grid_height - this->y;
                glRecti(this->x, inverted_y, this->x + 1, inverted_y - 1);
        }
};


std::vector<Ant> antsVec;


/*  IN: MATRIX = any (actually gets: GL_MODELVIEW)  */
/* OUT: MATRIX = GL_MODELVIEW                       */

GLvoid resizeGLScene(GLsizei width, GLsizei height)             // Resize and initialize the GL window
{
        if (height == 0)                                        // Prevent a divide by zero by
        {
                height = 1;                                     // Making height equal one
        }

        glMatrixMode(GL_PROJECTION);                            // Select the projection matrix

        glLoadIdentity();                                       // Reset the projection matrix

/*
        glOrtho( -16,32,
                -16,32,
                -9.0,9.0);
 */

        glOrtho(0.0,(double)grid_width,
                0.0,(double)grid_height,
                -9.0,9.0);


        glMatrixMode(GL_MODELVIEW);                                     // Select the modelview matrix

        glLoadIdentity();                                               // Reset the modelview matrix
}

void initGL()                                                           // All setup for OpenGL goes here
{
        glShadeModel(GL_SMOOTH);                                        // Enable smooth shading
        glClearColor(0.0f, 0.0f, 0.0f, 0.5f);                           // Black background
        glClearDepth(1.0f);                                             // Depth buffer setup
        glEnable(GL_DEPTH_TEST);                                        // Enables depth testing
        glDepthFunc(GL_LEQUAL);                                         // The type of depth testing to do
        glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);              // Really nice perspective calculations
}


/* Clear, then draw everything from scratch.
   This is expensive: lots of calls to glRectf */

/*  IN: MATRIX = GL_MODELVIEW   */
/* OUT: MATRIX = [unchanged]    */

void drawGLScene()                                                      // Here's where we do all the drawing
{
        // this is significant when we resize the window and fill the edges with black (rather than garbage)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);             // Clear screen and depth buffer

        glLoadIdentity();                                               // Reset the current modelview matrix

        // Draw the grid, but *not* ants.
        // Ant instances are responsible for drawing themselves.

        const unsigned int grid_width_MINUS_ONE = grid_width - 1;
        const unsigned int grid_height_MINUS_ONE = grid_height - 1;

        for (int x = grid_width_MINUS_ONE; x >= 0; --x) {                  // TODO regarding other backwards loop trouble, this seems to work
                for (int y = grid_height_MINUS_ONE; y >= 0; --y) {

                        const int inverted_y = grid_height - y; // avoid unsigned, to match OpenGL
                        const GLubyte intensity = grid[(x * grid_height) + y] ? 0 : 255;

                        glColor3ub(intensity,intensity,intensity);
                        if (intensity != 0) glRecti(x, inverted_y, x + 1, inverted_y - 1);

                }
        }
        for (int i = antsVec.size() - 1; i >= 0; --i) {
            antsVec.at(i).draw(); /////// move from safe at to unsafe []    or do proper iteration...
        }
}


inline void checkCommandLineArguments(const int argc, const char * argv[]) {
        if (argc != 3) {
            fputs("Usage: ants <image> <number of ants>\ne.g ants ./in.bmp 3\n", stderr);
			exit(1);
		}
}

int main(const int argc, const char * argv[])
{
	    checkCommandLineArguments(argc, argv); // exits if check fails
		number_of_ants = atoi(argv[2]);

        initImage(argv[1]);

        // Add the right number of randomly-placed ants:
        {
            const unsigned int numSquares = grid_width * grid_height;
//          std::default_random_engine random_engine;
            // inclusive at both ends:
//          std::uniform__int_distribution_<int> distribution(0, numSquares);

            antsVec.reserve(number_of_ants);

            // Track which squares are taken in an unsorted array. (A horror of complexity, but we assume few ants)
            int * /*int * const*/ alreadyTaken = (int *)alloca(sizeof(int) * number_of_ants); // use signed: we use -1 as tombstone
            memset(alreadyTaken, -1, sizeof alreadyTaken);

            for (int i = number_of_ants - 1; i >= 0; --i) {
//              int prospective = distribution( random_engine );

				#if ! ( defined WIN32 || defined __WIN32 || defined __WIN32__ )
                    srand (time(NULL));
					const int prospective = rand() % numSquares;
                #else
				    unsigned int prospective;
					rand_s( &prospective );
					prospective %= numSquares;
                #endif


//              printf("Randomly generated %d\n", prospective);
                bool isUnique = true;


                for (int j = number_of_ants - 1; j >= 0; --j) {  //// could do better than a full array scan each time...
                    if (alreadyTaken[j] == prospective) { isUnique = false; break; }
                }

                if (! isUnique) {
                    ++i; // we'll need to go round again to get the unique coordinate for this 'i'
                } else {
                    alreadyTaken[i] = prospective;
                    Ant newAnt(prospective / grid_height,
						       prospective % grid_height);

//                  printf("prospective: %d\n",prospective);
//                  printf("x:%d y:%d\n", newAnt.x, newAnt.y);
                    antsVec.push_back(newAnt);
                }
            }
        }

        // Create the main window
        sf::Window myWindow(sf::VideoMode(FIXED_WINDOW_WIDTH, FIXED_WINDOW_HEIGHT, 32), WINDOW_TITLE);

        //myWindow.setFramerateLimit(10);
        initGL();

        resizeGLScene(FIXED_WINDOW_WIDTH, FIXED_WINDOW_HEIGHT);

        drawGLScene();

        // Start 'game' loop
        while (myWindow.isOpen())
        {
                // Process events
                sf::Event event;
                while (myWindow.pollEvent(event))
                {
                        switch (event.type) {
                                // Close window when asked to (and exit)
                                case sf::Event::Closed:
                                        myWindow.close();
                                        break;

                                case sf::Event::Resized:
                                        resizeGLScene(event.size.width, event.size.height);
                                        drawGLScene();
                                        break;

                                // Handle keyboard events
                                case sf::Event::KeyPressed:
                                        switch (event.key.code) {
                                                case sf::Keyboard::P :

                                                    for (int j = antsVec.size() - 1; j >= 0; --j) {
                                                   //// fprintf(stderr,"%d\n",j);
                                                        Ant& currentAnt = antsVec.at(j);

                                                        for (unsigned int i = 0; i != NUM_MOVES_PER_KEYPRESS; ++i) {
                                                                    const int oldX = currentAnt.x; // coordinates *before* the move
                                                                    const int oldY = currentAnt.y;
                                                                    const int invertedY = grid_height - oldY;

                                                                    if (currentAnt.rotateAndMove() ) {
                                                                      //// TODO tidy up the erase call by using proper iterator?
                                                                      // fprintf(stderr,"REMOVED %d\n",j);

                                                                      // Yes, this insanity really is the proper way to remove the n'th element:
                                                                      antsVec.erase( antsVec.begin() + j );
                                                                      break;
/* TODO WHY ARE WE STILL SEEING A RED DOT*/                         }
/* FOR OUT-OF-BOUNDS ANTS? */                                       else { // only draw the ant in its new position if the move was within bounds
                                                                        currentAnt.draw();
                                                                    }

                                                                    // we're careful to get this intensity *after* the move
                                                                    const GLubyte intensity = grid[(oldX * grid_height) + oldY] ? 0 : 255;

                                                                    const int HIGHLIGHT_TRAVERSED = 0;

                                                                    glColor3ub(intensity - HIGHLIGHT_TRAVERSED, intensity, intensity);

                                                                    glRecti(oldX, invertedY, oldX + 1, invertedY - 1); // repaint the square we just left
                                                            }

                                                        }
                                                        break;

                                                case sf::Keyboard::Escape :
                                                        myWindow.close();
                                                        break;

                                                case sf::Keyboard::F1 :
                                                        fullscreen = !fullscreen;
                                                        myWindow.create(fullscreen ? sf::VideoMode::getDesktopMode() : sf::VideoMode(FIXED_WINDOW_WIDTH, FIXED_WINDOW_HEIGHT, 32),
                                                                        WINDOW_TITLE,
                                                                        (fullscreen ? sf::Style::Fullscreen : sf::Style::Resize | sf::Style::Close));
                                                        {
                                                                sf::Vector2u size = myWindow.getSize();
                                                                resizeGLScene(size.x,size.y);
                                                        }
                                                        drawGLScene(); // blank-slate needs a total re-draw
                                                        break;

                                                case sf::Keyboard::F5 :
                                                        vsync = !vsync;
                                                        break;
                                        }
                                        break;
                        }
                }

                myWindow.setVerticalSyncEnabled(vsync);

                // Set the active window before using OpenGL commands
                // It's useless here because active window is always the same,
                // but don't forget it if you use multiple windows or controls
                myWindow.setActive();

                /// in OS X, we need a full draw, in full-screen. (Double-buffering to blame?)
                if (fullscreen) {
                    drawGLScene();
                }

                // Finally, display rendered frame on screen
                myWindow.display();
        }

        delete[] grid;
        return EXIT_SUCCESS;
}

