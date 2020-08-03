
char receive_buffer[100];                              //>> Buffer to store characters received via UART PORT.
int receive_count    = 0;                              //>> Number of current characters stored in the buffer.
int command_issued   = 0;                              //>> Indicates if a command has been entered or not.
int timer_0_count    = 0;                              //>> Number of times timer has overflown.
int timer_0_overflow = 5;                              //>> Number of times timer must overflow before checking for command

//==> Initialises URT transmit and receive lines.
void start_UART() {

    PORTC = 0;                                         //>> Clear PORTC.
    TXSTA1.TX9  = 0;                                   //>> Enable 8-bit transmission mode.
    TXSTA1.TXEN = 1;                                   //>> Enable transmission.
    TXSTA1.SYNC = 0;                                   //>> Enable asynchronous mode.
    TXSTA1.BRGH = 0;                                   //>> Enable low speed baud mode.

    RCSTA1.SPEN = 1;                                   //>> Enable serial PORT.
    RCSTA1.RX9  = 0;                                   //>> Enable 8-bit receive mode.
    RCSTA1.FERR = 0;                                   //>> Enable framing error bit.
    RCSTA1.OERR = 0;                                   //>> Enable overrun error bit.
    RCSTA1.CREN = 1;                                   //>> Enable reception.
     
    BAUDCON1.BRG16 = 0;                                //>> Enable 8-bit baud generator mode.
    SPBRG1 = 12;                                       //>> Set baud rate (9600).

    TRISC6_bit = 0;                                    //>> Set PORTC<6> to OUTPUT. RC6 = EUSART Asynchronous Transmit.
    TRISC7_bit = 1;                                    //>> Set PORTC<7> as INPUT. RC7 = EUSART Asynchronous Receive.
}

//==> Starts timer zero. Checks if command has been entered every 0.15 seconds.
void start_T0() {
    T0CON.T0PS0   = 1;
    T0CON.T0PS1   = 1;                                 //>> Set PRESCALER to increment timer every 256 instructions.
    T0CON.T0PS2   = 1;
    T0CON.PSA     = 0;                                 //>> Turn PRESCALER ON.
    T0CON.T0CS    = 0;                                 //>> Set time clock source to internal clock.
    T0CON.T08BIT  = 1;                                 //>> Set timer to 8-bit mode (overflows at 256).
    INTCON.TMR0IE = 1;                                 //>> Enable interrupts from timer 0.
    INTCON.GIE    = 1;                                 //>> Enable global interrupts so that interrupt can reach CPU.
    T0CON.TMR0ON  = 1;                                 //>> Start the timer.
}

//==> Enables interrupts from EUSTART1.
void enable_rec_int() {
    PIE1.RC1IE  = 1;                                   //>> Enable receive interrupts.
    INTCON.PEIE = 1;                                   //>> Enable Peripheral Interrupts.
    INTCON.GIE  = 1;                                   //>> Enable Global Interrupts.
}

//==> Transmits a single character.
void send_char(unsigned char character) {
    while(TXSTA1.TRMT == 0);                           //>> Wait for transmission buffer (TSR) to empty. 1 = empty. 0 = full.
    TXREG1 = character;                                //>> Transmit character by placing it in TXREG.
}

//==> Transmits a string.
void send_string(char * string, int length) {
    int i;
    for(i = 0; i < length; i++) {
        while(TXSTA1.TRMT == 0);                       //>> Wait if data is currently being transmitted.
        TXREG1 = *(string + i);                        //>> Transmit next character.
    }
}

//==> Reads a single byte from EEPROM at specified address.
unsigned char read_byte_EEPROM(unsigned char EEPROM_address) {

    unsigned char byte;

    I2C1_Start();                                     //>> START condition. SDA line drops LO while SCL line is still HI (Inactive).
    I2C1_Wr(0xA0);                                    //>> First byte written tells I2C bus address of slave device and operation type (R / W). Write first.
    I2C1_Wr(EEPROM_address);                          //>> Second byte written specifies local address on slave device I want to operate on.
    I2C1_Stop();                                      //>> STOP condition. SDA line goes from LO to HI while SCL line is HI.
    I2C1_Start();                                     //>> START condition sent to start next operation.
    I2C1_Wr(0xA1);                                    //>> Specify slave device location plus read operation (1).
    byte = I2C1_Rd(0);                                //>> Read from previously specified EEPROM local address (EEPROM_address).
    I2C1_Stop();                                      //>> STOP condition. SDA line goes from LO to HI while SCL line is HI.
     
    return byte;

    //>> Write order: most significant byte first. Last byte specifies operation.
    //>> Read  0x50 (0101 0000). Shift >> 1 = (1010 0000, 0xA0) + 1 = 0xA1.
    //>> Write 0x50 (0101 0000). Shift >> 1 = (1010 0000, 0xA0) + 0 = 0xA0.
}

//==> Writes a single byte to EEPROM at specified address.
void write_byte_EEPROM(unsigned char byte, unsigned char write_addr) {

    I2C1_Start();                                    //>> Determines in I2C bus is free and sends START signal. No error -> returns 0.
    I2C1_Wr(0xA0);                                   //>> Send a byte via I2C bus. I2c must be configured (I2C1_Init) and START signal issues (I2C1_Start) before. Returns 0 if no errors.
    I2C1_wr(write_addr);                             //>> Write location on EEPROM. Address to write to on SLAVE device (second byte sent).
    I2C1_wr(byte);                                   //>> Send byte.
    I2C1_Stop();                                     //>> Send STOP signal.
}

//==> Parses read command inside read_bytes and executes it.
void execute_command() {
    
    unsigned char operation[4];                                  //>> Two operations: {READ, WRTE}.
    unsigned char target_address[1];                             //>> Max value: 0-9.

    unsigned char read_value;                                    //>> Read value: 0-9.
    unsigned char write_value[1];                                //>> Write value: 0-9.
     
    strncpy(operation, strtok(receive_buffer, " "), 4);          //>> Parse buffer. Get operation.
    strncpy(target_address, strtok(0, " "), 1);                  //>> Parse buffer. Get read address.

    target_address[0] = target_address[0] - '0';                 //>> Convert ASCII character to number.
    
    if (strncmp(operation, "READ", 4) == 0) {
        read_value = read_byte_EEPROM(target_address[0]);        //>> Read value from EEPROM at target address.
        send_char(read_value);                                   //>> Send retrieved value to PC via UART.
    } else if (strncmp(operation, "WRTE", 4) == 0) {
        strncpy(write_value, strtok(0, " "), 1);                 //>> Parse buffer. Get write value.
        write_byte_EEPROM(write_value[0], target_address[0]);    //>> Write value to EEPROM at target address.
    }
    receive_count = 0;                                           //>> Reset receive buffer for next command.p
}

//==> Main loop.
void main() {
    I2C1_Init(9600);                                             //>> Initialise PIC chip as I2C Master with baud rate of 9600.
    ANSELA = 0x00;                                               //>> Set all PORTS to DIGITAL mode.
    ANSELB = 0x00;
    ANSELC = 0x00;
    ANSELD = 0x00;
    ANSELE = 0x00;
  
    enable_rec_int();                                            //>> Enable receiver interrupts.
    start_UART();                                                //>> Initialise receiver and transmission lines for UART communication.
    start_T0();                                                  //>> Initialise and start timer 0.

    //>> send_string("Greetings!", sizeof("Greetings!"));        //>> Task 1 Demo. [UNCOMMENT].

    while(1) {}
}

//==> Interrupt subroutine.
interrupt() {

    unsigned char received_character;
    
    //>> Interrupt generated if receiver is enabled and there is an unread character in the receive queue (FIFO).
    if (PIR1.RC1IF == 1) {                                       //>> Data is ready (RC1IF flag set).
        PIR1.RC1IF = 0;                                          //>> Don't actually need this because it's auto cleared when reading RCREG. Read only status bit.
        received_character = RCREG1;                             //>> Get character from receive buffer.
        receive_buffer[receive_count++] = received_character;
        //>> send_char(received_character);                      //>> Task 2 Demo. [UNCOMMENT].
        
        if (RCSTA1.OERR) {                                       //>> If overrun error occurs.
            RCSTA1.CREN = 0;                                     //>> Disable receiver (clears overrun bit).
            RCSTA1.CREN = 1;                                     //>> Re-enable receiver.
        }
        
        if (RCSTA1.FERR) {                                       //>> If framing error occurs.
            RCSTA1.SPEN = 0;                                     //>> Disable serial PORT (clear framing error).
            RCSTA1.SPEN = 1;                                     //>> Re-activate serial PORT.
        }
        
        if (received_character == '.') {                         //>> Execute command if '.' character read.
           command_issued = 1;
        }
    }

     //>> Timer 0 interrupt generated on overflow.
    if (INTCON.TMR0IF == 1) {
        timer_0_count += 1;
        if (timer_0_count >= timer_0_overflow) {                 //>> Check if command available every 0.15 seconds.
            if (command_issued == 1) {
                execute_command();                               //>> Execute if available. [?]
                command_issued = 0;
                timer_0_count = 0;
            }
        }
        INTCON.TMR0IF = 0;
    }
}