#include <built_in.h>  // Include built-in functions
#include <string.h>    // Include string functions
#include <stdlib.h>    // For integer to string conversion

// Define Relay Pins
#define RELAY1_PIN PORTC.B0  // Relay 1 connected to RC0
#define RELAY2_PIN PORTC.B1  // Relay 2 connected to RC1

// Define LCD address
#define LCD_I2C_ADDRESS 0x27  // I2C address for the LCD

// Buffer for receiving data
char received_data[4];  // Command buffer (e.g., L10, L20)

// Variables for storing remaining time
unsigned int remaining_time = 0;
unsigned int timer_running = 0;  // To track if the timer is active

// Function prototypes
void I2C_LCD_Init();
void I2C_LCD_Write_Command(unsigned char cmd);
void I2C_LCD_Write_Character(unsigned char character);
void I2C_LCD_Write_String(char *text);
void I2C_LCD_Clear();
void UART_Read_Command();
unsigned int Extract_Time_From_Command();
void Display_Status();
void Display_Timer();
void Delay_Seconds(unsigned int seconds);

 void __interrupt() {
    if (INTCON.INTF) {   // Check INT0 interrupt flag
        PORTD.B2 = 1;    // Set RD2 high
        Delay_ms(500);   // Delay for visualization
        PORTD.B2 = 0;    // Reset RD2 low
        INTCON.INTF = 0; // Clear the interrupt flag
    }
}

void IntToStrCustom(int num, char *str) {
    int start=0;
    int end =0;
    int i = 0;
    int isNegative = 0;

    // Handle 0 explicitly
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    // Handle negative numbers
    if (num < 0) {
        isNegative = 1;
        num = -num;  // Make the number positive for further processing
    }

    // Process each digit
    while (num > 0) {
        str[i++] = (num % 10) + '0';  // Convert the last digit to character
        num = num / 10;
    }

    // Add negative sign if needed
    if (isNegative) {
        str[i++] = '-';
    }

    str[i] = '\0';  // Null-terminate the string

    // Reverse the string (since digits are in reverse order)
    start = 0;
    end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

// Custom case-insensitive string comparison
char stricmp(char *s1, char *s2) {
    while (*s1 && *s2) {
        char c1 = *s1 >= 'a' && *s1 <= 'z' ? *s1 - 32 : *s1;  // Convert to uppercase if needed
        char c2 = *s2 >= 'a' && *s2 <= 'z' ? *s2 - 32 : *s2;  // Convert to uppercase if needed

        if (c1 != c2) {
            return c1 - c2;  // Return difference
        }

        s1++;
        s2++;
    }
    return *s1 - *s2;  // Return difference at end of strings
}

void main() {
    // Configure RC0 and RC1 as output for relays
    TRISC.B0 = 0;
    TRISC.B1 = 0;


        // Disable analog inputs
    ANSEL = 0;  // Disable all analog inputs on PORTA
    ANSELH.B0 = 0; // Disable analog on RB0

    // Configure RB0 as input
    TRISB.B0 = 1;

    // Configure RD2 as output
    TRISD.B2 = 0;
    PORTD.B2 = 0; // Initialize RD2 to low

    // Configure interrupts
    OPTION_REG = 0b01111111; // Enable pull-ups and set rising edge
    INTCON.INTE = 1;         // Enable INT0 external interrupt
    INTCON.GIE = 1;          // Enable global interrupts

    
    // Initialize relays to OFF
    RELAY1_PIN = 0;
    RELAY2_PIN = 0;

    // Initialize UART and LCD
    UART1_Init(9600);  // UART at 9600 baud rate
    I2C1_Init(100000); // Initialize I2C at 100kHz
    Delay_ms(100);     // Wait for stabilization
    I2C_LCD_Init();    // Initialize LCD

    // Display initial message on LCD
    I2C_LCD_Clear();
    I2C_LCD_Write_Command(0x80);  // First row
    I2C_LCD_Write_String("F: OFF");
    I2C_LCD_Write_Command(0xC0);  // First row
    I2C_LCD_Write_String("L: OFF");

    while (1) {
        UART_Read_Command();  // Read command from Bluetooth
        // Check for timed commands (L10, L20, etc.)
        if (received_data[0] == 'L' && strlen(received_data) == 3 && isdigit(received_data[1]) && isdigit(received_data[2])) {
            remaining_time = Extract_Time_From_Command();
            if (remaining_time > 0) {
                timer_running = 1;  // Timer is running
                RELAY1_PIN = 1;     // Turn ON Relay 1
                Display_Status();   // Update display
                Display_Timer();    // Display timer on second row
                RELAY1_PIN = 0;     // Turn OFF Relay 1 after timer
                timer_running = 0;  // Timer stopped
                Display_Status();   // Update display after timer ends
            } else {
                I2C_LCD_Clear();
                I2C_LCD_Write_Command(0xC0); // Second row
                I2C_LCD_Write_String("Invalid Command");
            }
        }


  else if (stricmp(received_data, "LON") == 0) {
    RELAY1_PIN = 1;  // Turn ON Relay 1
    Display_Status();
} else if (stricmp(received_data, "LOF") == 0) {
    RELAY1_PIN = 0;  // Turn OFF Relay 1
    Display_Status();
} else if (stricmp(received_data, "FON") == 0) {
    RELAY2_PIN = 1;  // Turn ON Relay 2
    Display_Status();
} else if (stricmp(received_data, "FOF") == 0) {
    RELAY2_PIN = 0;  // Turn OFF Relay 2
    Display_Status();
} else {
    I2C_LCD_Clear();
    I2C_LCD_Write_Command(0xC0);  // Second row
    I2C_LCD_Write_String("Invalid Command");
}

}
}



void UART_Read_Command() {
    unsigned short index = 0;

    // Clear the buffer before reading
    memset(received_data, 0, sizeof(received_data));

    // Read characters until newline (`\n`) or buffer limit is reached
    while (index < sizeof(received_data) - 1) {
        while (!UART1_Data_Ready());  // Wait for data to be available
        received_data[index] = UART1_Read();  // Read the character

        // Stop reading if newline or carriage return is received
        if (received_data[index] == '\n' || received_data[index] == '\r') {
            break;
        }

        index++;
    }

    received_data[index] = '\0';  // Null-terminate the string
}


// Function to extract time from commands like L10, L20
unsigned int Extract_Time_From_Command() {
    if (received_data[0] == 'L' && isdigit(received_data[1]) && isdigit(received_data[2])) {
        return (received_data[1] - '0') * 10 + (received_data[2] - '0');  // Combine tens and ones digits
    }
    return 0;  // Return 0 if command is invalid
}

// Delay in seconds
void Delay_Seconds(unsigned int seconds) {
    while (seconds--) {
        Delay_ms(1000);  // Delay for 1 second
    }
}

// Function to display relay status on LCD (1st row for Relay 2, 2nd row for Relay 1)
void Display_Status() {
    I2C_LCD_Clear();

    // First row: Relay 2 status
    I2C_LCD_Write_Command(0x80);  // Move cursor to first row
    if (RELAY2_PIN == 1) {
        I2C_LCD_Write_String("F: ON ");
    } else {
        I2C_LCD_Write_String("F: OFF");
    }

    // Second row: Relay 1 status
    I2C_LCD_Write_Command(0xC0);  // Move cursor to second row
    if (RELAY1_PIN == 1) {
        I2C_LCD_Write_String("L: ON ");
    } else {
        I2C_LCD_Write_String("L: OFF");
    }
}

// Function to display timer countdown on LCD (second row)
void Display_Timer() {
    char timer_text[5];

    while (remaining_time > 0) {
        I2C_LCD_Clear();
        // Convert integer to string using custom function
        IntToStrCustom(remaining_time, timer_text);
        I2C_LCD_Write_Command(0xC0);  // Move cursor to second row
        I2C_LCD_Write_String("L:");
        I2C_LCD_Write_String(timer_text);  // Display the timer
        Delay_Seconds(1);  // Wait for 1 second
        remaining_time--;  // Decrease timer
    }

    I2C_LCD_Clear();
    I2C_LCD_Write_Command(0xC0);  // Move cursor to second row
    I2C_LCD_Write_String("L: DONE");  // Display DONE when the timer is complete
}

// Function to clear LCD
void I2C_LCD_Clear() {
    I2C_LCD_Write_Command(0x01);  // Clear display command
    Delay_ms(5);                  // Wait for execution
}

// LCD Initialization and related functions remain unchanged...
// (I2C_LCD_Init, I2C_LCD_Write_Command, I2C_LCD_Write_Character, I2C_LCD_Write_String)


// Initialize I2C LCD
void I2C_LCD_Init() {
    Delay_ms(20);  // Power-on delay
    I2C_LCD_Write_Command(0x33);  // Initialize sequence
    I2C_LCD_Write_Command(0x32);  // Initialize sequence
    I2C_LCD_Write_Command(0x28);  // Function set: 4-bit, 2-line
    I2C_LCD_Write_Command(0x0C);  // Display ON, Cursor OFF
    I2C_LCD_Write_Command(0x06);  // Entry mode set: Increment cursor
    I2C_LCD_Write_Command(0x01);  // Clear display
    Delay_ms(5);  // Command execution time
}

// Write command to LCD
void I2C_LCD_Write_Command(unsigned char cmd) {
    unsigned char upperNibble = cmd & 0xF0;  // Get upper nibble
    unsigned char lowerNibble = (cmd << 4) & 0xF0;  // Get lower nibble

    // Send upper nibble
    I2C1_Start();  // Start I2C communication
    I2C1_Wr(LCD_I2C_ADDRESS << 1);  // Send LCD address
    I2C1_Wr(upperNibble | 0x0C);  // Send data with enable bit
    I2C1_Wr(upperNibble | 0x08);  // Disable enable bit
    I2C1_Stop();  // Stop I2C communication

    // Send lower nibble
    I2C1_Start();  // Start I2C communication
    I2C1_Wr(LCD_I2C_ADDRESS << 1);  // Send LCD address
    I2C1_Wr(lowerNibble | 0x0C);  // Send data with enable bit
    I2C1_Wr(lowerNibble | 0x08);  // Disable enable bit
    I2C1_Stop();  // Stop I2C communication

    Delay_ms(2);  // Command execution time
}

// Write character to LCD
void I2C_LCD_Write_Character(unsigned char character) {
    unsigned char upperNibble = character & 0xF0;  // Get upper nibble
    unsigned char lowerNibble = (character << 4) & 0xF0;  // Get lower nibble

    // Send upper nibble
    I2C1_Start();  // Start I2C communication
    I2C1_Wr(LCD_I2C_ADDRESS << 1);  // Send LCD address
    I2C1_Wr(upperNibble | 0x0D);  // Send data with enable bit
    I2C1_Wr(upperNibble | 0x09);  // Disable enable bit
    I2C1_Stop();  // Stop I2C communication

    // Send lower nibble
    I2C1_Start();  // Start I2C communication
    I2C1_Wr(LCD_I2C_ADDRESS << 1);  // Send LCD address
    I2C1_Wr(lowerNibble | 0x0D);  // Send data with enable bit
    I2C1_Wr(lowerNibble | 0x09);  // Disable enable bit
    I2C1_Stop();  // Stop I2C communication

    Delay_ms(2);  // Data execution time
}

// Write string to LCD
void I2C_LCD_Write_String(char *text) {
    while (*text) {
        I2C_LCD_Write_Character(*text++);  // Write each character
    }
}
