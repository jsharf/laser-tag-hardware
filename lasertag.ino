enum PinNames {
  kLife1 = 8,
  kLife2 = 9,
  kLife3 = 10,
  kLaser = 5,
  kTrigger = 3,
};

const int kCoolDown_ms = 50;
const int kFirePeriod_ms = 50;
int kSafePeriod_ms = 3000;
int kSafeBlinkPeriod_ms = 200;

// Used for hit detection.
const int kNoiseThreshold = 80;

const int kNumReceivers = 6;
const int kReceiverPins[kNumReceivers] = {A0, A1, A2, A3, A4, A5};

struct SensorPacket {
  int data[kNumReceivers];
};

struct State {
  int number_lives;
  int invuln_period_since;
  bool invulnerable;
};

State global_state;

volatile uint16_t last_laser_fire = 0;
volatile uint16_t is_laser_cooled_down = true;
// Trigger switch interrupt handler (fire the laser).
void FireMahLaser() {
  if (is_laser_cooled_down && !global_state.invulnerable) {
    const int half_duty_cycle = 255/2;
    analogWrite(kLaser, half_duty_cycle);
    last_laser_fire = millis();
    is_laser_cooled_down = false;
  }
}

// Used for spike counting in loop().
int spike_count_since = 0;
// Setup routine.
void setup() {
  for (int i = 0; i < kNumReceivers; ++i) {
    pinMode(kReceiverPins[i], INPUT);
  }
  pinMode(kLaser, OUTPUT);
  pinMode(kTrigger, INPUT);
  pinMode(kLife1, OUTPUT);
  pinMode(kLife2, OUTPUT);
  pinMode(kLife3, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(3), FireMahLaser, RISING);

  spike_count_since = millis();

  global_state.number_lives = 3;
  global_state.invuln_period_since = millis();
  global_state.invulnerable = false;

  Serial.begin(57600);
}

void PollSensors(SensorPacket *p) {
  for (int i = 0; i < kNumReceivers; ++i) {
    p->data[i] = analogRead(kReceiverPins[i]);
  }
}

void PrintSensorPacket(const SensorPacket *p) {
  Serial.print(F("S: "));
  for (int i = 0; i < kNumReceivers; ++i) {
    // Print "i: p->data[i]"
    Serial.print(i); Serial.print(F(": "));
    Serial.print(p->data[i]);
  }
  // New line + Carriage return.
  Serial.println();
}

int SumSensorPacket(const SensorPacket *p) {
  int sum = 0;
  for (int i = 0; i < kNumReceivers; ++i) {
    sum += p->data[i];
  }
  return sum;
}

void DisplayLives() {
  const int lives = global_state.number_lives;
  digitalWrite(kLife3, (lives >= 3) ? HIGH : LOW);
  digitalWrite(kLife2, (lives >= 2) ? HIGH : LOW);
  digitalWrite(kLife1, (lives >= 1) ? HIGH : LOW);
  if (global_state.invulnerable) {
    const int time_offset = millis() - global_state.invuln_period_since;
    if (time_offset/(kSafeBlinkPeriod_ms/2) % 2 == 0) {
      digitalWrite(kLife3, LOW);
      digitalWrite(kLife2, LOW);
      digitalWrite(kLife1, LOW);
    }
  }
}

int last_sum = 0;
int spike_count = 0;  // relative to spike_count_since.
void loop() {
  // Read in and process sensor data.
  SensorPacket p;
  PollSensors(&p);
  PrintSensorPacket(&p);
  int sensor_sum = SumSensorPacket(&p);
  
  // Some dumb dsp... it works, I have no idea what I'm doing.
  const int delta = abs(sensor_sum - last_sum);
  last_sum = sensor_sum;
  if (delta > kNoiseThreshold) {
    spike_count++;
  }
  
  // Do maths to determine if we've been hit.
  // In 1 wave period you go up and down (square wave). that's two "derivative" spikes.
  const int waves = spike_count/2;
  // Oh god I'm doing such a terrible job explaining this. Sorry.
  // How long has it been since we last reset the spike count?
  const int time_period = millis() - spike_count_since;
  // The period of one wavelength is the total time we've been recording divided by the number of waves seen.
  const int period_ms = time_period/waves;
  // So we're expecting roughly a 500Hz signal. Say it's between 10Hz and 1KHz. I'd say that's good enough :).
  // Only try to check for this if we've been measuring for more than 50 ms.
  if ((period_ms < 3) && (period_ms > 1) && (time_period >= 10)) {
    if (!global_state.invulnerable) {
      // we've been hit scotty!
      spike_count = 0;
      spike_count_since = millis();
      global_state.invulnerable = true;
      --global_state.number_lives; 
      global_state.invuln_period_since = millis();
    }
  } else if (time_period > 50) {
    spike_count = 0;
    spike_count_since = millis();
  }

  if(millis() - global_state.invuln_period_since > kSafePeriod_ms) {
    global_state.invulnerable = false;
  }

  DisplayLives();

  if (global_state.number_lives <= 0) {
    // ded.
    detachInterrupt(digitalPinToInterrupt(3));
    // Print a debug msg.
    Serial.println(F("We regret to inform you that this laser gun is ded."));
    while(1) {
      // Paranoid anti-cheating measure. Makes sure laser is off.
      analogWrite(kLaser, 0);
    }
  }

  // Is it time to turn off the laser? Turn it off!
  uint16_t current_time = millis();
  if (current_time - last_laser_fire > kFirePeriod_ms) {
    analogWrite(kLaser, 0);
  }
  
  // Has the laser cooled down? Enable firing again.
  if (current_time - last_laser_fire > kCoolDown_ms) {
    is_laser_cooled_down = true;
  }
  
}
