#include "main.h"

void disabled(){}
void competition_initialize(){}

//Function to determine sign of a integer variable, returns bool
template <typename T> int sgn(T val){
    return (T(0) < val) - (val < T(0));
}

void serialRead(void* params){
    vexGenericSerialEnable(SERIALPORT - 1, 0);
    vexGenericSerialBaudrate(SERIALPORT - 1, 115200);
    pros::delay(10);
    pros::screen::set_pen(COLOR_BLUE);
    double distX, distY = 0;
    while(true){
        uint8_t buffer[256];
        int bufLength = 256;
        int32_t nRead = vexGenericSerialReceive(SERIALPORT - 1, buffer, bufLength);
        if(nRead >= 0){
            std::stringstream dataStream("");
            bool recordOpticalX, recordOpticalY = false;
            for(int i=0;i<nRead;i++){
                char thisDigit = (char)buffer[i];
                if(thisDigit == 'D' || thisDigit == 'I' || thisDigit == 'A' || thisDigit == 'X' || thisDigit == 'C' || thisDigit == 'Y'){
                    recordOpticalX = false;
                    recordOpticalY = false;
                }
                if(thisDigit == 'C'){
                    recordOpticalX = false;
                    dataStream >> distX;
                    pros::lcd::print(1, "Optical Flow:");
                    pros::lcd::print(2, "distX: %.2lf", distX/100);
                    dataStream.str(std::string());
                }
                if(thisDigit == 'D'){
                    recordOpticalY = false;
                    dataStream >> distY;
                    global_distY = distY/100;
                    pros::lcd::print(3, "distY: %.2lf", distY/100);
                    dataStream.str(std::string());
                }
                if (recordOpticalX) dataStream << (char)buffer[i];
                if (recordOpticalY) dataStream << (char)buffer[i];
                if (thisDigit == 'X') recordOpticalX = true;
                if (thisDigit == 'Y') recordOpticalY = true;
            }
        }
        pros::Task::delay(25);
    }
}

void brake(){
    luA.brake();
    ruA.brake();
    luB.brake();
    ruB.brake();
    llA.brake();
    rlA.brake();
    llB.brake();
    rlB.brake();
    pros::delay(1);
}

//Sets a deadzone for joystick inputs, defined radius is the DEADBAND constant
double apply_deadband(double value){
    return (fabs(value) > DEADBAND) ? value : 0.0;
}

//Sets a hard capped limit for motor RPM
double bound_value(double value){
    if (value > MAX_RPM) return MAX_RPM;
    if (value < -MAX_RPM) return -MAX_RPM;
    return value;
}

//Converts rotational sensor readings into degrees and bounds it between -180 to 180
double getNormalizedSensorAngle(pros::Rotation &sensor){
    double angle = sensor.get_angle() / 100.0; //Convert from centidegrees to degrees

    if (angle < -180)
        angle += 360;
    else if (angle > 180)
        angle -= 360;

    return angle;
}

//COME BACK BROOOOOOOO, GOT BUG
double getAngle(int x, int y){
    double a = std::atan2(std::abs(y), std::abs(x));

    if (x < 0){
        if (y < 0)
            return (M_PI * -1) + a;
        else if (y > 0)
            return M_PI - a;
        else
            return M_PI;
    }
    else if (x > 0){
        if (y < 0)
            return a * -1;
        else if (y > 0)
            return a;
        else
            return 0;
    }
    else{
        if (y < 0)
            return -M_PI / 2;
        else if (y > 0)
            return M_PI / 2;
        else
            return NAN; // return illegal value because a zero vector has no direction
    }
}

double closestAngle(double current_angle, double target_angle)
{
    //dir gives the angle from -180 to 180 degrees for the latest wheel angle
	double dir = std::fmod(current_angle, 360.0) - std::fmod(target_angle, 360.0);

	if (abs(dir) > 180.0)
		dir = -(sgn(dir) * 360.0) + dir; //flagged, this if statement is just a redundant implementation of wrapAngle(dir)

	return dir;
}

double wrapAngle(double angle){
    if (angle > 180.0)
        return angle -= 360.0;
    else if (angle < -180.0)
        return angle += 360.0;
    else
        return angle;
}

void swerveTranslation(){
	while(true){
		double translation_speed = sqrt(leftY * leftY + leftX * leftX);
		double wheel_target_angle = -getAngle(leftY, leftX) * TO_DEGREES;
		rotational = bound_value(rightX * SCALING_FACTOR); //currently as of 22.10.24 2000hrs the rotation functionality does not account for the fact that the effective width of the base changes when the swerve wheels change angle
        rotationalL = rotational;
        rotationalR = rotational;

        //Kicks in only when rotational joystick (right) is being used and when both wheels are 90degs
        //forces the wheels to not be collinear so that effective base width is nonzero and rotation can happen
        if(abs(leftY) < 16 && leftX < 0 && fabs(rotational) > 0){
            wheel_target_angle -= 25.0;
        }
        else if(abs(leftY) < 16 && leftX > 0 && fabs(rotational) > 0){
            wheel_target_angle += 25.0;
        }

		left_wheel_speed = translation_speed;
		right_wheel_speed = translation_speed;

        //Converts joystick input reading (-127 to 127) to motor RPM (-600 to 600) via a scaling factor
		left_wheel_speed = bound_value(left_wheel_speed * SCALING_FACTOR);
		right_wheel_speed = bound_value(right_wheel_speed * SCALING_FACTOR);
    
        //converts to degrees and bounds the angle from -180 to 180
		double left_sensor_angle = getNormalizedSensorAngle(left_rotation_sensor);
        double right_sensor_angle = getNormalizedSensorAngle(right_rotation_sensor);


		double setpointAngleL = closestAngle(left_sensor_angle, wheel_target_angle);
		double setpointAngleFlippedL = closestAngle(left_sensor_angle, wheel_target_angle + 180.0);

		double setpointAngleR = closestAngle(right_sensor_angle, wheel_target_angle);
		double setpointAngleFlippedR = closestAngle(right_sensor_angle, wheel_target_angle + 180.0);

		if(translation_speed > 0.0){

			if (abs(setpointAngleL) <= abs(setpointAngleFlippedL)){
                if(left_wheel_speed < 0.0){
                    left_wheel_speed = -left_wheel_speed;
                    isLeftFlipped = false;
                }
				target_angleL = (left_sensor_angle + setpointAngleL);
			}
			else{
                if(left_wheel_speed > 0.0){
                    left_wheel_speed = -left_wheel_speed;
                    isLeftFlipped = true;
                }
				target_angleL = (left_sensor_angle + setpointAngleFlippedL);
			}

			if (abs(setpointAngleR) <= abs(setpointAngleFlippedR)){
                if(right_wheel_speed < 0.0){
                    right_wheel_speed = -right_wheel_speed;
                    isRightFlipped = false;
                }
				target_angleR = (right_sensor_angle + setpointAngleR);
			}
			else{
                if(right_wheel_speed > 0.0){
                    right_wheel_speed = -right_wheel_speed;
                    isRightFlipped = false;
                }
				target_angleR = (right_sensor_angle + setpointAngleFlippedR);
			}
		}
		pros::Task::delay(4);
	}
}

//Pivots the wheels to its intended target angle
void setWheelAngle(){
	double left_previous_error = 0.0;
	double right_previous_error = 0.0;
	double left_integral = 0.0;
	double right_integral = 0.0;
	while(true){
		double left_current_angle = getNormalizedSensorAngle(left_rotation_sensor);
		double right_current_angle = getNormalizedSensorAngle(right_rotation_sensor);
        pros::lcd::print(4, "leftA:%lf", left_current_angle);
        pros::lcd::print(5, "rightA:%lf", right_current_angle);

		double left_error = wrapAngle(target_angleL - left_current_angle);
        double right_error = wrapAngle(target_angleR - right_current_angle);

		left_integral += left_error;
		right_integral += right_error;

		double left_derivative = left_error - left_previous_error;
		double right_derivative = right_error - right_previous_error;

		left_previous_error = left_error;
		right_previous_error = right_error;

		double left_motor_speed = lkP * left_error + lkI * left_integral + lkD * left_derivative;
		double right_motor_speed = rkP * right_error + rkI * right_integral + rkD * right_derivative;

		left_turn_speed = left_motor_speed;
		right_turn_speed = right_motor_speed;

		if(fabs(left_error) <= 5.0){
			left_turn_speed = 0.0;
			left_integral = 0.0;
			left_error = 0.0;
			left_derivative = 0.0;
		}
		if(fabs(right_error) <= 5.0){
			right_turn_speed = 0.0;
			right_integral = 0.0;
			right_error = 0.0;
			right_derivative = 0.0;
		}
		pros::Task::delay(4);
	}
}

void mogoLift(){
	double liftIntegral = 0.0;
	double liftDerivative = 0.0;
	double prevLiftError = 0.0;
	while(true){
		if(liftEnable){
			liftTarget = 305;
			solenoid.set_value(0);
		}
		else{
			liftTarget = 1010;
			solenoid.set_value(1);
		}

		int liftValue = lifter.get_value();
		pros::lcd::print(0, "Potentiometer Value: %d", liftValue);

		int liftError = abs(liftValue - liftTarget);
		liftIntegral += liftError;
		liftDerivative = liftError - prevLiftError;

		int liftPower = mkP * liftError + mkI * liftIntegral + mkD * liftDerivative;

		//liftPower = bound_value(liftPower * SCALING_FACTOR);

		if(liftPower > 127) liftPower = 127;
        else if(liftPower < -127) liftPower = -127;

		prevLiftError = liftError;

		if(abs(liftError) < 5){
            liftL.brake();
            liftR.brake();
			liftPower = 0.0;
            liftIntegral = 0.0;
            liftDerivative = 0.0;
        }
		else{
            if(liftValue > liftTarget){
                liftL.move(-liftPower);
                liftR.move(-liftPower);
            }
            else if(liftValue < liftTarget){
                liftL.move(liftPower);
                liftR.move(liftPower);
            }
        }
		pros::Task::delay(12);
	}
}

void goalIntake(){
	while(true){
		if(master.get_digital(DIGITAL_R1)){
            intakeLower.move(120);
            intakeUpper.move(120);
        }
		else if(master.get_digital(DIGITAL_R2)){
            intakeLower.move(-120);
            intakeUpper.move(-120);
        }
        else{
            intakeLower.move(0);
            intakeUpper.move(0);
        }
        pros::Task::delay(25);
	}
}

void moveBase(){
    while(true){
        //this case handles rotation plus translation simultaneously
        if(fabs(left_wheel_speed) > 0.0 && fabs(right_wheel_speed) > 0.0){
            std::cout << left_wheel_speed << std::endl;
            std::cout << right_wheel_speed << std::endl;
            //Does not tally with the math, all translational speeds should be scaled by 3
            double lu = 3*left_wheel_speed - left_turn_speed + rotational;
            double ll = 3*left_wheel_speed + left_turn_speed + rotational;
            double ru = 3*right_wheel_speed - right_turn_speed - rotational;
            double rl = 3*right_wheel_speed + right_turn_speed - rotational;

            //check if move_velocity is going to cap any of our motor motions, and if it will, 
            //then we need to scale all motions down enough such that move_velocity doesnt cap them, 
            //so that relative proportions of the motor motions is still the desired proportion even if the absolute amounts are scaled down.
            if(abs(lu) > 600 || abs(ll) > 600 || abs(ru) > 600 || abs(rl) > 600)
            {
                double max = lu;
                if(ll > max)
                    max = ll;
                if(ru > max)
                    max = ru;
                if(rl > max)
                    max = rl;
                double safetyScalingFactor = max / 600;
                lu = lu * safetyScalingFactor;
                ll = ll * safetyScalingFactor;
                ru = ru * safetyScalingFactor;
                rl = rl * safetyScalingFactor;
            }
            luA.move_velocity(lu);
            luB.move_velocity(lu);

            llA.move_velocity(ll);
            llB.move_velocity(ll);

            ruA.move_velocity(ru);
            ruB.move_velocity(ru);

            rlA.move_velocity(rl);
            rlB.move_velocity(rl);
            pros::lcd::print(6,"LEFT WHEEL AND RIGHT WHEEL");
        }
        //for POINT TURNS, so we have to force the wheels to be at zero degrees because we have bling wheels
        else if(fabs(left_wheel_speed) <= 2.0 && fabs(rotational) > 0.0){
            target_angleL = 0.0;
            target_angleR = 0.0;
            double lu = -left_turn_speed + rotational;
            double ll = left_turn_speed + rotational;
            double ru = -right_turn_speed - rotational;
            double rl = right_turn_speed - rotational;

            //check if move_velocity is going to cap any of our motor motions, and if it will, 
            //then we need to scale all motions down enough such that move_velocity doesnt cap them, 
            //so that relative proportions of the motor motions is still the desired proportion even if the absolute amounts are scaled down.
            if(abs(lu) > 600 || abs(ll) > 600 || abs(ru) > 600 || abs(rl) > 600)
            {
                double max = lu;
                if(ll > max)
                    max = ll;
                if(ru > max)
                    max = ru;
                if(rl > max)
                    max = rl;
                double safetyScalingFactor = max / 600;
                lu = lu * safetyScalingFactor;
                ll = ll * safetyScalingFactor;
                ru = ru * safetyScalingFactor;
                rl = rl * safetyScalingFactor;
            }
            luA.move_velocity(lu);
            luB.move_velocity(lu);

            llA.move_velocity(ll);
            llB.move_velocity(ll);

            ruA.move_velocity(ru);
            ruB.move_velocity(ru);

            rlA.move_velocity(rl);
            rlB.move_velocity(rl);
            pros::lcd::print(6,"NO TRANSLATE. ROTATE");
        }
        //this case handles wheel pivoting only
        else{
            luA.move_velocity(0);
            luB.move_velocity(0);

            llA.move_velocity(0);
            llB.move_velocity(0);

            ruA.move_velocity(0);
            ruB.move_velocity(0);

            rlA.move_velocity(0);
            rlB.move_velocity(0);
            pros::lcd::print(6,"ELSE CASE ONLY LEH");
        }
        pros::Task::delay(4);
    }
}

void movetotarget(double targetDist){
    double Integral = 0.0;
	double Derivative = 0.0;
	double prevError = 0.0;
    double kp = 4, ki = 0.0, kd = 0.0;
    double left_previous_error = 0.0;
	double right_previous_error = 0.0;
	double left_integral = 0.0;
	double right_integral = 0.0;
    while(true){
        double Error = fabs(targetDist - (global_distY - global_errorY));
        Integral += Error;
		Derivative = Error - prevError;
        int motorPower = kp * Error;
        if((fabs(Error) <= 1.0) || ((global_distY - global_errorY) >= targetDist)){
            brake();
            motorPower = 0.0;
            luA.move(motorPower);
            llA.move(motorPower);
            ruA.move(motorPower);
            rlA.move(motorPower);
            luB.move(motorPower);
            llB.move(motorPower);
            ruB.move(motorPower);
            rlB.move(motorPower);
            break;
        }
        else{
            target_angleL = 0.0;
            target_angleR = 0.0;
            luA.move_velocity(motorPower - left_turn_speed);
            llA.move_velocity(motorPower + left_turn_speed);
            ruA.move_velocity(motorPower - right_turn_speed);
            rlA.move_velocity(motorPower + right_turn_speed);
            luB.move_velocity(motorPower - left_turn_speed);
            llB.move_velocity(motorPower + left_turn_speed);
            ruB.move_velocity(motorPower - right_turn_speed);
            rlB.move_velocity(motorPower + right_turn_speed);
        }
        prevError = Error;
    }
}

void autonomous(){
    target_angleL = 0.0;
    target_angleR = 0.0;
    pros::delay(200);
    global_errorY = global_distY;
    movetotarget(61.0);
}

void initialize(){
	pros::lcd::initialize();
	luA.set_brake_mode(MOTOR_BRAKE_BRAKE);
    luB.set_brake_mode(MOTOR_BRAKE_BRAKE);
    llA.set_brake_mode(MOTOR_BRAKE_BRAKE);
    llB.set_brake_mode(MOTOR_BRAKE_BRAKE);
    ruA.set_brake_mode(MOTOR_BRAKE_BRAKE);
    ruB.set_brake_mode(MOTOR_BRAKE_BRAKE);
    rlA.set_brake_mode(MOTOR_BRAKE_BRAKE);
    rlB.set_brake_mode(MOTOR_BRAKE_BRAKE);
    liftL.set_brake_mode(MOTOR_BRAKE_HOLD);
    liftR.set_brake_mode(MOTOR_BRAKE_HOLD);

	while(!left_rotation_sensor.reset());
    while(!right_rotation_sensor.reset());

	left_rotation_sensor.set_data_rate(5);
    right_rotation_sensor.set_data_rate(5);

	left_rotation_sensor.set_position(0);
    right_rotation_sensor.set_position(0);

	imu.reset(true);
    imu.set_data_rate(5);

    lifter.calibrate();

	pros::Task swerve_translation(swerveTranslation);
    pros::Task set_wheel_angle(setWheelAngle);
    pros::Task move_base(moveBase);
    pros::Task mogo_lift(mogoLift);
	pros::Task goal_intake(goalIntake);
    pros::Task serial_read(serialRead);

	master.clear();
}

void opcontrol(){
	while(true){
		leftX = apply_deadband(master.get_analog(ANALOG_LEFT_X));
        leftY = apply_deadband(master.get_analog(ANALOG_LEFT_Y));
        rightX = apply_deadband(master.get_analog(ANALOG_RIGHT_X));

		if(master.get_digital_new_press(DIGITAL_X)) liftEnable = !liftEnable;
        if(master.get_digital_new_press(DIGITAL_A)) pros::Task start(autonomous);

        // luA.move_velocity(rotational);
        // llA.move_velocity(rotational);
        // ruA.move_velocity(-rotational);
        // rlA.move_velocity(-rotational);
        // luB.move_velocity(rotational);
        // llB.move_velocity(rotational);
        // ruB.move_velocity(-rotational);
        // rlB.move_velocity(-rotational);

		pros::delay(5);
	}
}