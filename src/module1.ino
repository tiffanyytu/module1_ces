/**************************************************************************
  STATES OF MATTER
  Generative Art Installation — "Transitions"

  Hardware:
    - ESP32 TTGO / LILYGO T-Display
    - TFT_eSPI library

  Concept:
    A group of particles continuously transitions through three states:

        SOLID  ->  LIQUID  ->  GAS  ->  SOLID ...

    SOLID:
      Particles arrange themselves into a rigid grid and gently vibrate.

    LIQUID:
      Particles break free from the grid, fall under "gravity", and flow
      around each other near the bottom of the screen.

    GAS:
      Particles spread throughout the entire screen and move quickly
      in random directions.

    The transitions happen gradually instead of cutting instantly from
    one state to another.

    Random movement makes every cycle slightly different, so this is
    generative rather than a repeating GIF.
**************************************************************************/

#include <TFT_eSPI.h>
#include <SPI.h>
#include <esp_system.h>

// ------------------------------------------------------------
// DISPLAY
// ------------------------------------------------------------

TFT_eSPI tft = TFT_eSPI();

// We draw everything onto a sprite first.
// This greatly reduces flickering on the screen.
TFT_eSprite canvas = TFT_eSprite(&tft);


// ------------------------------------------------------------
// PARTICLE SETTINGS
// ------------------------------------------------------------

// 48 particles = 8 columns x 6 rows in the solid state.
const int NUM_PARTICLES = 48;
const int GRID_COLS = 8;
const int GRID_ROWS = 6;

// Radius of each particle.
const int PARTICLE_RADIUS = 3;

// Keep particles slightly away from the edge of the display.
const int EDGE_MARGIN = 4;


// Each particle remembers its:
//
// x, y       = current position
// vx, vy     = current velocity
// solidX/Y   = where it belongs when the material becomes solid
//
struct Particle {
  float x;
  float y;

  float vx;
  float vy;

  float solidX;
  float solidY;
};

Particle particles[NUM_PARTICLES];


// ------------------------------------------------------------
// STATES OF MATTER
// ------------------------------------------------------------

enum MatterState {
  SOLID,
  LIQUID,
  GAS
};

// We begin as a solid.
MatterState currentState = SOLID;

// The next state will be liquid.
MatterState nextState = LIQUID;


// ------------------------------------------------------------
// TIMING
// ------------------------------------------------------------

// How long the particles stay fully in one state.
const unsigned long HOLD_TIME = 6000;

// How long it takes to transition into the next state.
const unsigned long TRANSITION_TIME = 3500;

// Time when the current phase began.
unsigned long phaseStartTime = 0;

// Are we currently moving between two states?
bool transitioning = false;


// ------------------------------------------------------------
// OPTIONAL TEXT
// ------------------------------------------------------------

// Set this to true if you want the state name shown on screen.
// For the final installation, I think it looks better as false.
const bool SHOW_LABEL = false;


// ------------------------------------------------------------
// HELPER: RANDOM FLOAT
// ------------------------------------------------------------

// Arduino's random() normally returns integers.
// This helper gives us a random decimal number between minValue
// and maxValue.
float randomFloat(float minValue, float maxValue) {

  float randomNumber = random(0, 10001) / 10000.0;

  return minValue + randomNumber * (maxValue - minValue);
}


// ------------------------------------------------------------
// HELPER: SMOOTH TRANSITION
// ------------------------------------------------------------

// Instead of moving linearly from 0 -> 1, this creates a smoother
// ease-in/ease-out transition.
//
// At the beginning and end, the transition slows down.
float smoothStep(float t) {

  // Keep t between 0 and 1.
  t = constrain(t, 0.0f, 1.0f);

  return t * t * (3.0f - 2.0f * t);
}


// ------------------------------------------------------------
// HELPER: BLEND TWO TFT COLORS
// ------------------------------------------------------------

// TFT colors are stored as RGB565 values.
//
// This function lets us gradually transition from one color
// to another.
//
// amount = 0.0 --> completely color1
// amount = 1.0 --> completely color2
uint16_t blendColors(uint16_t color1, uint16_t color2, float amount) {

  amount = constrain(amount, 0.0f, 1.0f);

  // Break color 1 into red, green, and blue components.
  int r1 = (color1 >> 11) & 0x1F;
  int g1 = (color1 >> 5) & 0x3F;
  int b1 = color1 & 0x1F;

  // Break color 2 into red, green, and blue components.
  int r2 = (color2 >> 11) & 0x1F;
  int g2 = (color2 >> 5) & 0x3F;
  int b2 = color2 & 0x1F;

  // Interpolate between the colors.
  int r = r1 + (r2 - r1) * amount;
  int g = g1 + (g2 - g1) * amount;
  int b = b1 + (b2 - b1) * amount;

  // Turn RGB back into RGB565.
  return (r << 11) | (g << 5) | b;
}


// ------------------------------------------------------------
// COLOR FOR EACH STATE
// ------------------------------------------------------------

uint16_t getStateColor(MatterState state) {

  if (state == SOLID) {
    return TFT_CYAN;
  }

  if (state == LIQUID) {
    return TFT_MAGENTA;
  }

  // GAS
  return TFT_ORANGE;
}


// ------------------------------------------------------------
// CREATE SOLID GRID
// ------------------------------------------------------------

// Assign every particle a fixed location.
//
// The target positions form an evenly spaced grid.
//
// When the particles become solid, they are pulled toward these
// locations like springs.
void createSolidTargets() {

  float xSpacing = tft.width() / (float)(GRID_COLS + 1);
  float ySpacing = tft.height() / (float)(GRID_ROWS + 1);

  for (int i = 0; i < NUM_PARTICLES; i++) {

    int column = i % GRID_COLS;
    int row = i / GRID_COLS;

    particles[i].solidX = (column + 1) * xSpacing;
    particles[i].solidY = (row + 1) * ySpacing;
  }
}


// ------------------------------------------------------------
// INITIALIZE PARTICLES
// ------------------------------------------------------------

void initializeParticles() {

  createSolidTargets();

  for (int i = 0; i < NUM_PARTICLES; i++) {

    // Begin each particle almost exactly where it belongs
    // in the solid grid.
    particles[i].x =
      particles[i].solidX + randomFloat(-1.0, 1.0);

    particles[i].y =
      particles[i].solidY + randomFloat(-1.0, 1.0);

    particles[i].vx = 0;
    particles[i].vy = 0;
  }
}


// ------------------------------------------------------------
// START A TRANSITION
// ------------------------------------------------------------

void startTransition() {

  transitioning = true;
  phaseStartTime = millis();

  // Decide which state comes next.
  if (currentState == SOLID) {

    nextState = LIQUID;

    // Give particles a small push as the solid starts melting.
    for (int i = 0; i < NUM_PARTICLES; i++) {
      particles[i].vx += randomFloat(-0.8, 0.8);
      particles[i].vy += randomFloat(-0.2, 0.5);
    }
  }

  else if (currentState == LIQUID) {

    nextState = GAS;

    // Give particles a much stronger burst of energy
    // as the liquid becomes gas.
    for (int i = 0; i < NUM_PARTICLES; i++) {

      particles[i].vx += randomFloat(-2.0, 2.0);
      particles[i].vy += randomFloat(-2.5, 0.5);
    }
  }

  else {

    // GAS -> SOLID
    nextState = SOLID;

    // Slightly shift the solid grid each cycle.
    // This means the "new solid" is not perfectly identical
    // every time.
    createSolidTargets();

    for (int i = 0; i < NUM_PARTICLES; i++) {

      particles[i].solidX += randomFloat(-2.0, 2.0);
      particles[i].solidY += randomFloat(-2.0, 2.0);
    }
  }
}


// ------------------------------------------------------------
// UPDATE STATE MACHINE
// ------------------------------------------------------------

void updateState() {

  unsigned long now = millis();

  // If we are sitting fully in one state...
  if (!transitioning) {

    // ...begin transitioning after HOLD_TIME milliseconds.
    if (now - phaseStartTime >= HOLD_TIME) {
      startTransition();
    }
  }

  // If we are currently transitioning...
  else {

    // ...finish the transition after TRANSITION_TIME milliseconds.
    if (now - phaseStartTime >= TRANSITION_TIME) {

      currentState = nextState;

      transitioning = false;

      phaseStartTime = now;
    }
  }
}


// ------------------------------------------------------------
// CALCULATE STATE WEIGHTS
// ------------------------------------------------------------

// These variables describe "how much" of each state currently exists.
//
// Example halfway between SOLID and LIQUID:
//
//     solidAmount  = 0.5
//     liquidAmount = 0.5
//     gasAmount    = 0.0
//
// This allows the physics to smoothly change instead of snapping.
void calculateStateAmounts(
  float &solidAmount,
  float &liquidAmount,
  float &gasAmount
) {

  solidAmount = 0;
  liquidAmount = 0;
  gasAmount = 0;

  // Not currently transitioning:
  // one state gets 100%.
  if (!transitioning) {

    if (currentState == SOLID) {
      solidAmount = 1;
    }

    else if (currentState == LIQUID) {
      liquidAmount = 1;
    }

    else {
      gasAmount = 1;
    }

    return;
  }


  // Calculate transition progress from 0 -> 1.
  float progress =
    (millis() - phaseStartTime) / (float)TRANSITION_TIME;

  progress = smoothStep(progress);


  // The current state fades OUT.
  float oldAmount = 1.0 - progress;

  // The next state fades IN.
  float newAmount = progress;


  // Assign the correct weights.
  if (currentState == SOLID) {
    solidAmount += oldAmount;
  }

  else if (currentState == LIQUID) {
    liquidAmount += oldAmount;
  }

  else {
    gasAmount += oldAmount;
  }


  if (nextState == SOLID) {
    solidAmount += newAmount;
  }

  else if (nextState == LIQUID) {
    liquidAmount += newAmount;
  }

  else {
    gasAmount += newAmount;
  }
}


// ------------------------------------------------------------
// PARTICLE REPULSION
// ------------------------------------------------------------

// Liquid particles should not all collapse into the exact same point.
//
// This gives nearby particles a small repulsive force.
void applyParticleRepulsion(float strength) {

  const float minimumDistance = 8.0;

  for (int i = 0; i < NUM_PARTICLES; i++) {

    for (int j = i + 1; j < NUM_PARTICLES; j++) {

      float dx = particles[i].x - particles[j].x;
      float dy = particles[i].y - particles[j].y;

      float distanceSquared = dx * dx + dy * dy;

      // Ignore particles that are sufficiently far apart.
      if (
        distanceSquared <
        minimumDistance * minimumDistance &&
        distanceSquared > 0.01
      ) {

        float distance = sqrt(distanceSquared);

        float overlap = minimumDistance - distance;

        float force = overlap * strength;

        float forceX = (dx / distance) * force;
        float forceY = (dy / distance) * force;

        particles[i].vx += forceX;
        particles[i].vy += forceY;

        particles[j].vx -= forceX;
        particles[j].vy -= forceY;
      }
    }
  }
}


// ------------------------------------------------------------
// UPDATE PARTICLE PHYSICS
// ------------------------------------------------------------

void updateParticles() {

  float solidAmount;
  float liquidAmount;
  float gasAmount;

  calculateStateAmounts(
    solidAmount,
    liquidAmount,
    gasAmount
  );


  // ----------------------------------------------------------
  // PHYSICS PARAMETERS
  //
  // These automatically morph as we transition between states.
  // ----------------------------------------------------------

  // Solid particles strongly return to their grid positions.
  float springStrength =
    0.065 * solidAmount;

  // Liquid particles experience gravity.
  float gravity =
    0.055 * liquidAmount;

  // Random movement gets stronger as matter becomes less rigid.
  float randomMotion =
      0.006 * solidAmount
    + 0.030 * liquidAmount
    + 0.075 * gasAmount;

  // Solid particles lose velocity rapidly.
  // Gas particles preserve velocity.
  float damping =
      0.82  * solidAmount
    + 0.965 * liquidAmount
    + 0.997 * gasAmount;

  // Maximum speed also changes depending on state.
  float maxSpeed =
      0.8 * solidAmount
    + 1.8 * liquidAmount
    + 3.3 * gasAmount;


  // Liquid particles push each other apart more strongly.
  float repulsion =
      0.015 * solidAmount
    + 0.080 * liquidAmount
    + 0.020 * gasAmount;


  // Apply particle-to-particle interaction.
  applyParticleRepulsion(repulsion);


  // ----------------------------------------------------------
  // UPDATE EACH PARTICLE
  // ----------------------------------------------------------

  for (int i = 0; i < NUM_PARTICLES; i++) {

    Particle &p = particles[i];


    // --------------------------------------------------------
    // SOLID BEHAVIOR
    // --------------------------------------------------------

    // Think of every particle as being connected to its grid
    // location by a tiny invisible spring.
    //
    // As solidAmount increases, the spring gets stronger.

    float dx = p.solidX - p.x;
    float dy = p.solidY - p.y;

    p.vx += dx * springStrength;
    p.vy += dy * springStrength;


    // --------------------------------------------------------
    // LIQUID BEHAVIOR
    // --------------------------------------------------------

    // Gravity pulls liquid particles downward.
    p.vy += gravity;

    // Create a gentle left/right current.
    //
    // The sine function makes the direction slowly shift,
    // creating flowing rather than purely random motion.
    float wave =
      sin(
        millis() * 0.002 +
        i * 0.45
      );

    p.vx += wave * 0.025 * liquidAmount;


    // --------------------------------------------------------
    // RANDOM MOLECULAR MOTION
    // --------------------------------------------------------

    // Solid = tiny vibration
    // Liquid = moderate movement
    // Gas = energetic movement

    p.vx += randomFloat(
      -randomMotion,
      randomMotion
    );

    p.vy += randomFloat(
      -randomMotion,
      randomMotion
    );


    // --------------------------------------------------------
    // DAMPING
    // --------------------------------------------------------

    p.vx *= damping;
    p.vy *= damping;


    // --------------------------------------------------------
    // LIMIT SPEED
    // --------------------------------------------------------

    float speed =
      sqrt(
        p.vx * p.vx +
        p.vy * p.vy
      );

    if (speed > maxSpeed) {

      p.vx = (p.vx / speed) * maxSpeed;
      p.vy = (p.vy / speed) * maxSpeed;
    }


    // --------------------------------------------------------
    // MOVE PARTICLE
    // --------------------------------------------------------

    p.x += p.vx;
    p.y += p.vy;


    // --------------------------------------------------------
    // SCREEN BOUNDARIES
    // --------------------------------------------------------

    // Left wall
    if (p.x < EDGE_MARGIN) {

      p.x = EDGE_MARGIN;
      p.vx *= -0.8;
    }

    // Right wall
    if (p.x > tft.width() - EDGE_MARGIN) {

      p.x = tft.width() - EDGE_MARGIN;
      p.vx *= -0.8;
    }

    // Top wall
    if (p.y < EDGE_MARGIN) {

      p.y = EDGE_MARGIN;
      p.vy *= -0.8;
    }

    // Bottom wall
    if (p.y > tft.height() - EDGE_MARGIN) {

      p.y = tft.height() - EDGE_MARGIN;

      p.vy *= -0.65;

      // Liquid loses a little horizontal velocity
      // when sliding along the bottom.
      p.vx *= (1.0 - 0.08 * liquidAmount);
    }
  }
}


// ------------------------------------------------------------
// DRAW EVERYTHING
// ------------------------------------------------------------

void drawScene() {

  // Clear the previous frame.
  canvas.fillSprite(TFT_BLACK);


  // Figure out how much of each state is currently visible.
  float solidAmount;
  float liquidAmount;
  float gasAmount;

  calculateStateAmounts(
    solidAmount,
    liquidAmount,
    gasAmount
  );


  // ----------------------------------------------------------
  // CHOOSE COLOR
  // ----------------------------------------------------------

  uint16_t particleColor;

  // If we are fully inside one state...
  if (!transitioning) {

    particleColor =
      getStateColor(currentState);
  }

  // If we are transitioning, smoothly blend the two colors.
  else {

    float progress =
      (millis() - phaseStartTime) /
      (float)TRANSITION_TIME;

    progress = smoothStep(progress);

    particleColor =
      blendColors(
        getStateColor(currentState),
        getStateColor(nextState),
        progress
      );
  }


  // ----------------------------------------------------------
  // DRAW PARTICLES
  // ----------------------------------------------------------

  for (int i = 0; i < NUM_PARTICLES; i++) {

    Particle &p = particles[i];


    // Add slight color variation between particles.
    //
    // Every third particle is mixed slightly toward white.
    uint16_t thisColor = particleColor;

    if (i % 3 == 0) {

      thisColor =
        blendColors(
          particleColor,
          TFT_WHITE,
          0.22
        );
    }


    // --------------------------------------------------------
    // SOLID VISUAL
    // --------------------------------------------------------

    // Larger, consistent circles emphasize the packed structure.
    int radius =
      PARTICLE_RADIUS;


    // Gas particles become slightly smaller,
    // emphasizing that they are dispersed.
    if (gasAmount > 0.5) {
      radius = 2;
    }


    canvas.fillCircle(
      (int)p.x,
      (int)p.y,
      radius,
      thisColor
    );


    // Add a tiny glowing center.
    if (radius >= 3) {

      canvas.drawPixel(
        (int)p.x,
        (int)p.y,
        TFT_WHITE
      );
    }
  }


  // ----------------------------------------------------------
  // OPTIONAL STATE LABEL
  // ----------------------------------------------------------

  if (SHOW_LABEL) {

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGREY);

    canvas.setCursor(5, 5);

    if (!transitioning) {

      if (currentState == SOLID) {
        canvas.print("SOLID");
      }

      else if (currentState == LIQUID) {
        canvas.print("LIQUID");
      }

      else {
        canvas.print("GAS");
      }
    }

    else {

      if (currentState == SOLID && nextState == LIQUID) {
        canvas.print("MELTING");
      }

      else if (currentState == LIQUID && nextState == GAS) {
        canvas.print("EVAPORATING");
      }

      else {
        canvas.print("SOLIDIFYING");
      }
    }
  }


  // Push the completed frame from memory onto the display.
  canvas.pushSprite(0, 0);
}


// ------------------------------------------------------------
// SETUP
// ------------------------------------------------------------

void setup() {

  // Initialize the T-Display.
  tft.init();

  // Landscape orientation.
  tft.setRotation(1);

  // Black screen while starting.
  tft.fillScreen(TFT_BLACK);


  // ----------------------------------------------------------
  // CREATE OFF-SCREEN DRAWING CANVAS
  // ----------------------------------------------------------

  // 16-bit color gives us the normal TFT color palette.
  canvas.setColorDepth(16);

  canvas.createSprite(
    tft.width(),
    tft.height()
  );


  // ----------------------------------------------------------
  // RANDOMNESS
  // ----------------------------------------------------------

  // esp_random() uses the ESP32's hardware random-number generator.
  // This helps make every power-on slightly different.
  randomSeed(esp_random());


  // Create our particle system.
  initializeParticles();


  // Begin our timing cycle.
  phaseStartTime = millis();
}


// ------------------------------------------------------------
// MAIN LOOP
// ------------------------------------------------------------

void loop() {

  // Check whether we should remain in the current state
  // or begin/finish a transition.
  updateState();

  // Apply the physics for this frame.
  updateParticles();

  // Draw the resulting particle positions.
  drawScene();

  // About 40 frames per second.
  //
  // A short delay also prevents the animation from running
  // unnecessarily fast.
  delay(25);
}
