// TribeDuino: A proof of concept that uses an Arduino to read a Korg
// Monotribe firmware file in m4a audio format.
//
// By Mike Tsao (http://github.com/sowbug)
//
// See included README for instructions and credits.

#include <stdarg.h>

#include <Arduino.h>

// The number of packets we should expect to see in this firmware file.
const uint8_t TOTAL_PACKETS = 129;

// This makes analogRead() go faster.
// Found at http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1208715493/11
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

// http://www.arduino.cc/playground/Main/Printf
void p(const char *fmt, ... ) {
  char tmp[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(tmp, 128, fmt, args);
  va_end(args);
  Serial.print(tmp);
}

void setup() {
  Serial.begin(57600);

  // Set the ADC's reference voltage to 1.1v.
  //
  // If we wanted to get serious about this, we'd provide an AREF voltage
  // that's significantly lower to increase the useful resolution. We'd
  // probably also look into an amplifier or voltage divider to get the actual
  // input range from 0..N volts (as it is, I think we're chopping off
  // negative voltages). 
  analogReference(INTERNAL);

  // Blink LED to show that we're reading a packet.
  pinMode(13, OUTPUT);

  // Set prescale to 16.
  // See http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1208715493/11
  sbi(ADCSRA,ADPS2);
  cbi(ADCSRA,ADPS1);
  cbi(ADCSRA,ADPS0);
}

// These were determined through empirical analysis.
const int LOW_THRESHOLD = 0;
const int HIGH_THRESHOLD = 128;

inline bool readBit() {
  while (true) {
    int a = analogRead(A0);
    if (a <= LOW_THRESHOLD) {
      return false;
    }
    if (a >= HIGH_THRESHOLD) {
      return true;
    }
  }
}

inline void waitForFallingEdge() {
  while (!readBit()) {
  }
  while (readBit()) {
  }
}

inline void waitForRisingEdge() {
  while (readBit()) {
  }
  while (!readBit()) {
  }
}

// Determines the carrier frequency. Takes advantage of the ~2-second run
// of high bits at the start of the sequence.
//
// Rainy-day activity: regenerate a sine wave using Audacity and see whether
// your Arduino can keep up. How high a bitrate can you achieve?
//
// See http://en.wikipedia.org/wiki/Frequency-shift_keying if you don't know
// what carrier frequency means. And if you do figure out what it means,
// please tell me; I'm only beginning to understand it. The trick is that the
// data signal modulates the carrier's _frequency_, not amplitude. For some
// reason when I was trying to conceptualize it, I kept adding together the
// data/carrier waves in my head (i.e., amplitude modulation), and it wasn't
// working out.
float determine_frequency() {
  const int SAMPLE_COUNT = 128;
  unsigned long samples[SAMPLE_COUNT];

  for (int i = 0; i < SAMPLE_COUNT; ++i) {
    unsigned long t = micros();
    waitForRisingEdge();
    waitForFallingEdge();
    samples[i] = micros() - t;
  }
  float total = 0;
  for (int i = 0; i < SAMPLE_COUNT; ++i) {
    total += samples[i];
  }
  float average = total / SAMPLE_COUNT;
  float frequency = 1000000.0 / average;
  return frequency;
}

// As a convenience to the user, wait until something happens on the
// microphone before starting to sample.
void wait_for_sound() {
  for (int i = 0; i < 32; ++i) {
    waitForRisingEdge();
    waitForFallingEdge();
  }
}

bool is_weird_packet(uint8_t packet_number) {
  return packet_number == 0 || packet_number == TOTAL_PACKETS;
}

void loop() {
  p("\n\nReady for sound file... ");
  wait_for_sound();
  p("Determining carrier frequency... ");

  const float frequency = determine_frequency();
  const unsigned long usec = 1000000 / frequency;
  const unsigned long usec_low = usec * 75 / 100;
  const unsigned long usec_high = usec * 150 / 100;

  p("%dHz\n", (int)frequency);

  p("Now reading %d packets...\n", TOTAL_PACKETS);
  wait_for_sound();

  enum {
    READ_SYNC = 0,
    READ_PROLOGUE,
    READ_PACKET,
    READ_EPILOGUE,
  };
  uint8_t state = READ_SYNC;

  uint8_t packets_read = 0;

  uint16_t simple_checksum = 0;
  uint16_t total_packet_bytes_read = 0;

  uint8_t byte = 0;  // The register into which we shift bits.
  int bit_count = 0;  // How many bits we've shifted into the byte.

  const int PACKET_SIZE = 256;  // Size of the data packet.
  uint8_t bytes[PACKET_SIZE];
  unsigned long byte_count = 0;

  unsigned long t = micros();
  waitForRisingEdge();
  while (true) {
    waitForFallingEdge();

    // Figure out how long it's been since the last falling edge. Knowing this
    // number is the essence of BFSK. If it's above a certain number, that
    // means we read a zero. Otherwise it's a one. (Or vice-versa, depending
    // on the protocol.)
    unsigned long now = micros();
    unsigned long s = now - t;
    t = now;
    bool is_short = s > usec_low && s < usec_high;

    // READ_SYNC solves the problem of not knowing which bit we're at relative
    // to the start of the next byte. We sync up by reading a bunch of ones,
    // then start shifting into the byte register after reading a zero.
    if (state == READ_SYNC) {
      if (!is_short) {
        state = READ_PROLOGUE;
      }
      continue;
    }

    // Shift the bit into the register.
    byte = byte >> 1;
    byte |= is_short ? 0x80 : 0;

    // Have we completed a byte?
    if (++bit_count == 8) {
      bit_count = 0;

      if (state == READ_PROLOGUE) {
        if (byte != 0xA9) {
          p("Error: expected READ_PROLOGUE 0xA9, but got %02x.\n", byte);
          while (true) {
          }
        }

        // Prologue was good. Flip on the LED and switch to the packet-reading
        // state.
        digitalWrite(13, HIGH);
        state = READ_PACKET;

        continue;
      }

      // The remaining states all want to remember the bytes read, so store
      // the one we just completed in the byte buffer.
      bytes[byte_count++] = byte;

      if (state == READ_PACKET) {

        // To further confirm that our proof of concept works, we use a
        // very simple (and quick) checksum.
        //
        // For firmware 0201, decoded length 33024 bytes and
        // shasum 4f1cbd041a166565b654c1ebe50562bf380305d4, the output should
        // be 16330, calculated with the Python script included with this file.
        // We exclude so-called "weird" packets from the checksum.
        //
        // We don't count the first/last packet in the checksum or total bytes,
        // because they aren't considered part of the firmware file.
        bool is_weird = is_weird_packet(packets_read);
        if (!is_weird) {
          simple_checksum += byte;
          ++total_packet_bytes_read;
        }

        // Have we completed a packet?
        if (byte_count == PACKET_SIZE) {
          digitalWrite(13, LOW);
          byte_count = 0;
          if (is_weird) {
            // For some reason, the first packet (the one containing
            // "KORG SYSTEM FILE") and the last packet don't have an epilogue.
            // So go straight to waiting for the next packet to start.
            state = READ_SYNC;
          } else {
            state = READ_EPILOGUE;
          }

          if (++packets_read == TOTAL_PACKETS) {
            p("Successfully read %d packets. Checksum %u. "
              "Firmware bytes %u.\n", packets_read, simple_checksum,
              total_packet_bytes_read);
            while (true) {
            }
          }
          continue;
        }
      }

      // Have we completed the epilogue?
      if (state == READ_EPILOGUE && byte_count == 5) {
        byte_count = 0;

        // If this program were doing any actual work, then during the next
        // 342 bits that make up the sync sequence, we'd use that time (about
        // 77 milliseconds) to empty out the 256-byte buffer, probably by
        // flashing that block to the firmware. But we're just proving a
        // concept, so we'll laze around waiting for the sync bit instead.
        state = READ_SYNC;

        if (bytes[0] != 0x55 || bytes[1] != 0x55 || bytes[2] != 0x55) {
          p("Error: expected epilogue, but got %02x%02x%02x.\n", bytes[0],
            bytes[1], bytes[2]);
          while (true) {
          }
        }

        // bytes[3] is the checksum, we think. It varies.

        if (bytes[4] != 0xFF) {
          // The 0xFF isn't strictly speaking part of the epilogue, but we know
          // that every packet should be followed by a sync phase.
          p("Error: expected 0xFF after epilogue, but got %02x.\n", bytes[4]);
          while (true) {
          }
        }
        continue;
      }

    }
  }
}
