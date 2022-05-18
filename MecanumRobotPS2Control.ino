// NOTE: requires the Encoder library.
// 1) open Tools -> Manage Libraries...
// 2) install "Encoder" by Paul Stoffregen v1.4.1
#include <Encoder.h>

// NOTE: Requires the PS2X_lib installed.
// 1) open Sketch -> Include Library -> Add .ZIP Library
// 2) select "PS2X_lib.zip"
#include <PS2X_lib.h>
 
//PS2
#define PS2_DAT        52  //14    
#define PS2_CMD        51  //15
#define PS2_SEL        53  //16
#define PS2_CLK        50  //17

// #define pressures   true
#define pressures   false
#define rumble      true
//#define rumble      false
PS2X ps2x; // create PS2 Controller Class

int error = 0;
byte type = 0;
byte vibrate = 0;

void (* resetFunc) (void) = 0;

// --- SPD Motor ---

class SPDMotor {
  public:
  SPDMotor( int encoderA, int encoderB, bool encoderReversed, int motorPWM, int motorDir1, int motorDir2 );

  /// Set the PWM speed and direction pins.
  /// pwm = 0, stop (no active control)
  /// pwm = 1 to 255, proportion of CCW rotation
  /// pwm = -1 to -255, proportion of CW rotation
  void speed( int pwm );

  /// Activate a SHORT BRAKE mode, which shorts the motor drive EM, clamping motion.
  void hardStop();

  /// Get the current speed.
  int getSpeed();

  /// Get the current rotation position from the encoder.
  long getEncoderPosition();

  private:
    Encoder *_encoder;
    bool _encoderReversed;
    int _motorPWM, _motorDir1, _motorDir2;

    // Current speed setting.
    int _speed;
};

SPDMotor::SPDMotor( int encoderA, int encoderB, bool encoderReversed, int motorPWM, int motorDir1, int motorDir2 ) {
  _encoder = new Encoder(encoderA, encoderB);
  _encoderReversed = encoderReversed;

  _motorPWM = motorPWM;
  pinMode( _motorPWM, OUTPUT );
  _motorDir1 = motorDir1;
  pinMode( _motorDir1, OUTPUT );
  _motorDir2 = motorDir2;
  pinMode( _motorDir2, OUTPUT );
}

/// Set the PWM speed and direction pins.
/// pwm = 0, stop (no active control)
/// pwm = 1 to 255, proportion of CCW rotation
/// pwm = -1 to -255, proportion of CW rotation
void SPDMotor::speed( int speedPWM ) {
  _speed = speedPWM;
  if( speedPWM == 0 ) {
    digitalWrite(_motorDir1,LOW);
    digitalWrite(_motorDir2,LOW);
    analogWrite( _motorPWM, 255);
  } else if( speedPWM > 0 ) {
    digitalWrite(_motorDir1, LOW );
    digitalWrite(_motorDir2, HIGH );
    analogWrite( _motorPWM, speedPWM < 255 ? speedPWM : 255);
  } else if( speedPWM < 0 ) {
    digitalWrite(_motorDir1, HIGH );
    digitalWrite(_motorDir2, LOW );
    analogWrite( _motorPWM, (-speedPWM) < 255 ? (-speedPWM): 255);
  }
}

/// Activate a SHORT BRAKE mode, which shorts the motor drive EM, clamping motion.
void SPDMotor::hardStop() {
    _speed = 0;
    digitalWrite(_motorDir1,HIGH);
    digitalWrite(_motorDir2,HIGH);
    analogWrite( _motorPWM, 0);
}

/// Get the current speed.
int SPDMotor::getSpeed() {
    return _speed;
}

/// Get the current rotation position from the encoder.
long SPDMotor::getEncoderPosition() {
  long position = _encoder->read();
  return _encoderReversed ? -position : position;
}

SPDMotor *motorLF = new SPDMotor(18, 31, true, 12, 34, 35); // <- Encoder reversed to make +position measurement be forward.
SPDMotor *motorRF = new SPDMotor(19, 38, false, 8, 36, 37); // <- NOTE: Motor Dir pins reversed for opposite operation
SPDMotor *motorLR = new SPDMotor( 3, 49, true,  6, 43, 42); // <- Encoder reversed to make +position measurement be forward.
SPDMotor *motorRR = new SPDMotor( 2, A1, false, 5, A4, A5); // <- NOTE: Motor Dir pins reversed for opposite operation

void setup()
{
  Serial.begin(9600);
  delay(300) ;//added delay to give wireless ps2 module some time to startup, before configuring it
  //CHANGES for v1.6 HERE!!! **************PAY ATTENTION*************

  //setup pins and settings: GamePad(clock, command, attention, data, Pressures?, Rumble?) check for error
  error = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, pressures, rumble);

  if (error == 0) {
    Serial.println("Found Controller, configuration successful ");
    Serial.println();
    Serial.println("SPDMotor control by Aaron Hilton of Steampunk Digital");
    Serial.println("=====================================================");
    Serial.println("Holding L1 or R1 will activate analog joystick control.");
    Serial.println("Left analog stick for forward/back and turning.");
    Serial.println("Right analog stick for sideways movement.");
    Serial.println("Hold both L1 and R1 for full-speed.");
  }
  else if (error == 1)
  {
    Serial.println("No controller found, check PS2 receiver is inserted the correct way around.");
    resetFunc();
  }
  else if (error == 2)
    Serial.println("Controller found but not accepting commands.");

  else if (error == 3)
    Serial.println("Controller refusing to enter Pressures mode, may not support it. ");

  type = ps2x.readType();
  switch (type) {
  case 0:
    Serial.println("Unknown Controller type found ");
    break;
  case 1:
    Serial.println("DualShock Controller found ");
    break;
  case 2:
    Serial.println("GuitarHero Controller found ");
    break;
  case 3:
    Serial.println("Wireless Sony DualShock Controller found ");
    break;
  }
}


void loop() {
  if (error == 1) //skip loop if no controller found
    return;

  if (type == 2) { //Guitar Hero Controller
    return;
  }
  else  { //DualShock Controller
    static long oldSumPosition = 0;
    long sumPosition = motorLF->getEncoderPosition() + motorRF->getEncoderPosition() + motorLR->getEncoderPosition() + motorRR->getEncoderPosition();
    long deltaPosition = sumPosition - oldSumPosition;
    oldSumPosition = sumPosition;
    Serial.print("∆pos: ");
    Serial.print(deltaPosition);
    long posVibrate = abs(deltaPosition);
    posVibrate = posVibrate > 255 ? 255 : 0;
    vibrate = (byte)posVibrate;
    ps2x.read_gamepad(false, vibrate); //read controller and set large motor to spin at 'vibrate' speed

    if (ps2x.Button(PSB_L1) || ps2x.Button(PSB_R1)) {
      int LY = ps2x.Analog(PSS_LY);
      int LX = ps2x.Analog(PSS_LX);
      int RX = ps2x.Analog(PSS_RX);
      float forwardNormalized = (float)(-LY + 128)/127.f;

      forwardNormalized = constrain( forwardNormalized, -1.f, 1.f );
      float multiplier = (ps2x.Button(PSB_L1) && ps2x.Button(PSB_R1)) ? 255.f : 80.f;
      int forward = (int)(pow(forwardNormalized, 2.0) * multiplier);
  
      // Preserve the direction of movement.
      if( forwardNormalized < 0 ) {
        forward = -forward;
      }
   
      int right = -RX + 127;
      int ccwTurn = (LX - 127)/2;
      Serial.print( " fwd:" );
      Serial.print( forward );
      Serial.print( " r:" );
      Serial.print( right );
      Serial.print( " ∆°:" );
      Serial.print( ccwTurn );
      Serial.print( " LF:" );
      Serial.print( motorLF->getEncoderPosition() );
      Serial.print( " RF:" );
      Serial.print( motorRF->getEncoderPosition() );
      Serial.print( " LR:" );
      Serial.print( motorLR->getEncoderPosition() );
      Serial.print( " RR:" );
      Serial.println( motorRR->getEncoderPosition() );
  
      motorLF->speed(forward + ccwTurn - right); motorRF->speed(forward - ccwTurn + right);
      motorLR->speed(forward - ccwTurn - right); motorRR->speed(forward + ccwTurn + right);
    } else {
      // If there's motor power, try to hard-stop briefly.
      if( motorLF->getSpeed() != 0
      || motorRF->getSpeed() != 0
      || motorLR->getSpeed() != 0
      || motorRR->getSpeed() != 0 )
      {
          motorLF->hardStop(); motorRF->hardStop();
          motorLR->hardStop(); motorRR->hardStop();
          delay(500);
          motorLF->speed(0); motorRF->speed(0);
          motorLR->speed(0); motorRR->speed(0);
      }
    }
  }
}
