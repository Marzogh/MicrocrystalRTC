// Defines
//----------------------------------------------------------------------------------//
// 					Library defaults to 100kHz I2C bus.								//
// Uncomment the define below only if 400kHz I2C is required throughout the library //
// 			Use i2c_highSpeed() or i2c_lowSpeed() to switch speeds in software		//
//																					//
//#define HIGHSPEED											 						//
//																					//
//----------------------------------------------------------------------------------//
#ifdef HIGHSPEED
#define SCL_FREQ 400000L	//I2C clock is set to 400kHz
#elif
#define SCL_FREQ 100000L	//I2C clock is set to 100kHz
#endif

#define SLA_W(address)	(address << 1)
#define SLA_R(address)	((address <<1) + 0x01)

// Header files to include
#include <stdint.h>
#include <inttypes.h>
#include <util/twi.h>
#include <i2c.h>

//Variables

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//     			Private functions used for I2C operations     		  //
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

//Resets the I2C bus if a transaction lockUp occurs
static void _lockUp(void) {
	TWCR &= 0x00;									//Turns off I2C bus
	TWCR = _BV(TWEN) | _BV(TWEA);					//Reinitializes the I2C bus
}

//Sends address
static uint8_t _sendAddress(uint8_t _address) {

	TWDR = _address;
	TWCR = _BV(TWINT) | _BV(TWEN);					//Sends device address

	while (!(TWCR & _BV(TWINT)));					//Wait until the transmission finishes

	//Checks the value of the TWI status register for Master Write Slave ACK
	//(or) Master Read Slave ACK after masking the pre-scaler bits
	if ((TW_STATUS == TW_MT_SLA_ACK) || (TW_STATUS == TW_MR_SLA_ACK)) {
		return 0;
	}

	if ((TW_STATUS == TW_MT_SLA_NACK) || (TW_STATUS == TW_MR_SLA_NACK)) {
		i2c_stop()
		return TW_STATUS;
	}
	else {
		_lockUp();
		return TW_STATUS;
	}
}

//Sends byte to slave
static uint8_t _send(uint8_t _data) {

	TWDR = _data;
	TWCR = _BV(TWINT) | _BV(TWEN);					//Sends data byte

	while (!(TWCR & _BV(TWINT)));					//Wait until the transmission finishes

	if (TW_STATUS == TW_MT_DATA_ACK) {
		return 0;
	}

	if (TW_STATUS == TW_MT_DATA_NACK) {
		i2c_stop();
		return TW_STATUS;
	}
	else {
		_lockUp();
		return TW_STATUS;
	}
}

//Receives byte from slave
static uint8_t _receive(uint8_t _ack) {

	if (_ack) {
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWEA);
	}
	else {
		TWCR = _BV(TWINT) | _BV(TWEN);
	}

	while (!(TWCR & _BV(TWINT)));					//Wait until the transmission finishes

	if (TW_STATUS = TW_MR_ARB_LOST) {
		_lockUp();
		return TW_STATUS;
	}
	return TW_STATUS;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//     			Public functions used for I2C operations 		      //
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

//Initializes the I2C clock at 100kHz and sets the prescaler to 1. Then, enables the I2C bus
void i2c_begin(void) {

	TWSR &= 0xFC;									//Prescaler of 1
	TWBR = ((F_CPU/SCL_FREQ)-16)/2;					//Refer to 'Bit Rate Generator Unit' in datasheet

	TWCR = _BV(TWEN) | _BV(TWEA);					//Enables I2C bus and ACKs
}

//Disables and releases the I2C bus
void i2c_end(void) {
	TWCR &= 0x00;
}

//Issues a Start condition. Returns 0 if successful and non-zero if unsuccessful.
uint8_t i2c_start(void) {
	uint8_t TW_STATUS;

	TWCR = (_BV(TWINT) | _BV(TWSTA) | _BV(TWEN));	//Sets the START condition

	while (!(TWCR & _BV(TWINT)));					//Wait until the transmission finishes

	//Checks the value of the TWI status register after masking the pre-scaler bits
	if ((TW_STATUS == TW_START) || (TW_STATUS == TW_REP_START)) {				//If TWI status register reads START (or) REP_START
		return 0;
	}

	if ((TW_STATUS == TW_MT_ARB_LOST) || (TW_STATUS == TW_MR_ARB_LOST)) {		//If TWI status register reads ARB_LOST
		_lockUp();
		return(TW_STATUS);
	}

	return (TW_STATUS);
}

//Stops I2C transaction by issuing a STOP condition
uint8_t i2c_stop(void) {

	TWCR = (_BV(TWINT) | _BV(TWEN)| _BV(TWSTO));	//Sets the STOP condition

	while (!(TWCR & _BV(TWINT)));					//Wait until the transmission finishes

	//Checks the value of the TWI status register after masking the pre-scaler bits
	if (TW_STATUS != TW_STOP) {						//If TWI status register does not read TW_STOP
		_lockUp();
		return TW_STATUS;
	}

	return TW_STATUS;
}

//Sets up low speed (100kHz) I2C clock
void i2c_lowSpeed() {
	TWBR = ((F_CPU/100000L)-16)/2;					//Refer to 'Bit Rate Generator Unit' in datasheet
}

//Sets up high speed (400kHz) I2C clock
void i2c_highSpeed() {
	TWBR = ((F_CPU/400000L)-16)/2;					//Refer to 'Bit Rate Generator Unit' in datasheet
}

//Writes a byte of data to a particular device address. Must be followed by an i2c_stop f not followed by a consecutive write.
uint8_t i2c_write(uint8_t address, uint8_t data) {
	uint8_t status = 0;

	status = i2c_start();
	if (status) {
		return status;
	}

	status = _sendAddress(SLA_W(address));
	if (status) {
		return status;
	}

	status = _send(data);
	if (status) {
		return status;
	}

	return status;
}

//Writes data to a particular register address on a particular device
uint8_t i2c_write2Reg(uint8_t address, uint8_t regAddress, uint8_t *data, uint8_t numberOfBytes) {
	uint8_t status = 0;

	status = i2c_write(address, regAddress);
	if (status) {
		return status;
	}

	for (uint8_t i = 0; i < numberOfBytes; i++) {

		status = _send(data[i]);
		if (status) {
			return status;
		}

	}

	status = i2c_stop();
	(status) ? return status : continue;
	}

	return status;
}

/*Another way??
//Writes data to a particular register address on a particular device
uint8_t i2c_write2Reg(uint8_t address, uint8_t regAddress, uint8_t *data) {
	uint8_t status = 0;

	status = i2c_write(address, regAddress);
	if (status) {
		return status;
	}

	uint8_t numberOfBytes = sizeof(data);
	
	for (uint8_t i = 0; i < numberOfBytes; i++) {

		status = _send(data[i]);
		if (status) {
			return status;
		}

	}

	status = i2c_stop();
	(status) ? return status : continue;
	}

	return status;
}*/