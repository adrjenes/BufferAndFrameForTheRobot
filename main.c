/* USER CODE BEGIN Header */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ringbuff.h"
#include "protocol.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
/* USER CODE END Includes */
/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef uint8_t bool;
/* USER CODE END PTD */
/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */
/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */
/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

volatile buffer_t Tx = {0}, Rx = {0}; // Bufory kołowe
char frameData[100]; // Dane ramki z pola payload (dane)
char command[3]; // Komenda ramki
int badVal[] = {0x00, 0x23};
bool processing = true;
uint8_t frame[sizeOfBuffer]; // Ramka
uint8_t tempstring[CHECKSUM_LEN] = "";
uint8_t frameChar = '\0'; // Pojedynczy znak ramki
uint8_t frameLength = 0; // Długość ramki
uint8_t frameReceiving = 0;
uint8_t checksum = 0;
uint8_t l_checksum = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void broadcastingFrame(char data[]);
void tooLongFrame();
void badFrame();
void badChecksum();
void commandsReaction();
/* USER CODE BEGIN PFP */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == USART2) {

		Rx.RXbuffIdx++;
		if (Rx.RXbuffIdx >= sizeOfBuffer) {
			Rx.RXbuffIdx = 0;
		}
		HAL_UART_Receive_IT(&huart2, &Rx.array[Rx.RXbuffIdx], 1);
	}
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
	if(Tx.TXbuffIdx != Tx.RXbuffIdx){
		uint8_t tempChar = Tx.array[Tx.TXbuffIdx];
		Tx.TXbuffIdx++;

		if(Tx.TXbuffIdx >= sizeOfBuffer){
			Tx.TXbuffIdx = 0;
		}
		HAL_UART_Transmit_IT(&huart2, &tempChar, 1);
	}
}


void USART_Send(char* message, ...){
	char tempMessage[105];
	int i;
	volatile int send_idx = Tx.RXbuffIdx;
	va_list arglist;
	va_start(arglist, message);
	vsprintf(tempMessage, message, arglist);
	va_end(arglist);
	for (i = 0; i < strlen(tempMessage); i++) {
		Tx.array[send_idx] = tempMessage[i];
		send_idx++;
		if (send_idx >= sizeOfBuffer) {
			send_idx = 0;
		}}
	__disable_irq();
	if ((Tx.RXbuffIdx == Tx.TXbuffIdx) && (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE)==SET)) {
		Tx.RXbuffIdx = send_idx;
		uint8_t tmp = Tx.array[Tx.TXbuffIdx];
		Tx.TXbuffIdx++;
		if (Tx.TXbuffIdx >= sizeOfBuffer)
			Tx.TXbuffIdx = 0;
		HAL_UART_Transmit_IT(&huart2, &tmp, 1);
		} else {
			Tx.RXbuffIdx = send_idx;
		}
	__enable_irq();
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void infoFrame(){ // Ramka, wywolywanie komendy
	USART_Send("Komenda: ");
	USART_Send(command);
	USART_Send("\r\n");
}
void badCommand(){ // Ramka, wywolywanie bledu
	char message[105] = "Nie znaleziono podanej komendy.";
	USART_Send(message);
	USART_Send("\r\n");
}

void badFrame(){ // Ramka, wywolywanie bledu
	char message[105] = "Error! Znaleziono niedozwolone znaki.";
	USART_Send(message);
	USART_Send("\r\n");
}
void badChecksum(){ // Ramka, wywolywanie bledu
	char message[105] = "Error! Nieprawidlowa checksuma.";
	USART_Send(message);
	USART_Send("\r\n");
}
uint8_t checkCommandFrame(){

	//Sprawdzanie czy wszystkie 3 znaki koemendy to literki
	for (int i=0;i<3;i++){
		if(!isalpha((unsigned char)command[i])){
			badFrame();
			return 0;
		}
	}
	return 1;
}
void commandsReaction(){ // Ramka, analiza i wykonanie polecenia
	if (checkCommandFrame()) {
		if (command[0] == 'B'  && command[1] == 'F' && command[2] == 'F'){ infoFrame(); }
		else if (command[0] == 'B' && command[1] == 'F' && command[2] == 'L'){ infoFrame(); }
		else if (command[0] == 'I' && command[1] == 'D' && command[2] == 'X'){ infoFrame(); }
		else if (command[0] == 'C' && command[1] == 'L' && command[2] == 'R'){ infoFrame(); }
		else { badCommand(); USART_Send("\r\n"); }
	}
}
void broadcastingFrame(char data[]) { // Ramka, nadawanie
	char message[105];
	strcat(message,data);
	strcat(message,";");
	USART_Send(message);
}

void tooLongFrame(){ // Ramka, reagowanie na przekroczenie limitu znakow
	char message[105] = "Error! Przekroczono limit znakow ramki.";
	USART_Send(message);
	USART_Send("\r\n");
}
void tooShortFrame(){ // Ramka, reagowanie na przekroczenie limitu znakow
	char message[105] = "Error! Minimalna dlugosc ramki to 7 znakow.";
	USART_Send(message);
	USART_Send("\r\n");
}
void notEndFrame(){ // Ramka, reagowanie na przekroczenie limitu znakow
	char message[105] = "Error! Brak znaku konca ramki.";
	USART_Send(message);
	USART_Send("\r\n");
}
/* USER CODE END 0 */
/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */
  /* MCU Configuration--------------------------------------------------------*/
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  /* USER CODE BEGIN Init */
  /* USER CODE END Init */
  /* Configure the system clock */
  SystemClock_Config();
  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  /* USER CODE END 2 */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  HAL_UART_Receive_IT(&huart2, &Rx.array[Rx.RXbuffIdx], 1);

  while (1)
  {
	  if(Rx.ReceivedCharIdx != Rx.RXbuffIdx) 								// Sprawdzamy czy jest jakiś znak do odebrania.
	  	{
		  frameChar = Rx.array[Rx.ReceivedCharIdx]; 						// Pobieramy znak z bufora.
		  Rx.ReceivedCharIdx++; 											// Przechodzimy na kolejne miejsce.

		  if(Rx.ReceivedCharIdx >= sizeOfBuffer) 							// Sprawdzamy czy wskaźnik nie wykracza poza bufor.
		      {
			  	 Rx.ReceivedCharIdx = 0; 									// Jeśli tak, to ustawiamy go na początek bufora.
		      }

		  if(frameChar == 0x24) 											// Sprawdzamy czy znak początku ramki to $
              {
                 frameReceiving = 1; 	 									// Jeśli tak, to odbieranie na 1
                 frameLength = 0; 											// Jeśli tak to długość ramki na 0
              }
		  if(frameReceiving == 1) 											// Jeśli wykryliśmy znak początku
		  	  {
		  		 frame[frameLength] = frameChar; 							// To pobieramy znak do ramki
		  		 frameLength++; 											// I przechodzimy dalej
		  		 if (frameLength > 108) 									// Sprawdzamy czy ramka nie jest za długa
		  		 	 {
		  				tooLongFrame(); 									// Reagujemy na zbyt dluga ramke
		  				frameReceiving = 0; 								// Odbieranie na 0
		  				frameLength = 0; 									// Dlugosc ramki na 0
		  		 	 }
		  if(frameChar == 0x23) 											// Sprawdzamy znak konca ramki #
		  	 {
			  if(frameLength > 8)											// Sprawdzamy minimalna dlugosc ramki
			  	 {
		  			 for (int i = 1; i <= 7; i++) {
		  				if ((!frame[i]) || frame[i] == 0x23)   				// Sprawdzamy czy dane miejsce nie jest puste lub nie wystepuje znak # w tym miejscu
		  				{													// Jesli wykryjemy brak znaku lub znak konca ramki
		  				   processing = false;								// to nie bedziemy wysylac ramki
		  				}
		  			 }
		  			 for (int i = 4; i < 7; i++) {							// Sprawdzamy czy na pozycji
		  			    if (!isdigit(frame[i])) {							// znakow checksumy znajduja sie znaki liczb
		  			      processing = false;								// Jesli nie to nie bedziemy wysylac ramki
		  			    }
		  			 }
		  			 if (processing)
		  				{
		  				   memcpy(command, &frame[1], 3); 					// Kopiujemy komende do zmiennej command
		  				   memcpy(tempstring, &frame[4], 3); 				// Kopiujemy checksume do zmiennej tempstring
		  				   checksum = atoi((const char *)tempstring); 	    // Konwertujemy checksume na wartosc decymalna
		  				   for (int offset = 0; offset < 3; ++offset)
		  				   {
		  					   l_checksum += command[offset];				// Konwertujemy pobrane znaki z command do zmiennej l_checksum
		  				   }
		  				   if (l_checksum == checksum)
		  				   {
		  					   memcpy(frameData, &frame[7], 100); 			// Kopiujemy znaki z ramki do zmiennej frameData
		  					   commandsReaction(); 							// Wywolujemy funkcje odpowiedzialna za wykonanie komendy
		  					   USART_Send("\r\n");
		  				   }
		  				   else
		  				   {
		  					   badChecksum();								// Reagujemy na bledna checksume
		  				   }
		  				}
		  				   else
		  				   {
		  					   badFrame();									// Reagujemy na bledna komende
		  				   }
					   	   memset(frameChar, 0, 100); 						// Czyscimy dane ramki
					   	   tempstring[CHECKSUM_LEN] = 0;					// Czyscimy zapasowej tablicy na 0
					   	   l_checksum = 0;									// Dlugosc policzonej checksumy na 0
					   	   checksum = 0;									// Dlugosc checksumy na 0
					   	   frameLength = 0; 								// Dlugosc ramki na 0
					   	   frameReceiving = 0;								// Zmienna wykrywajaca znak poczatku ramki na 0
					   	   processing = true;								// Zmienna odpowiedzialna za proces na 0

		  		} else {
		  			tooShortFrame();
		  		}
		  	 }
		  }
	   }
   }
    /* USER CODE BEGIN 3 */
  	  /* USER CODE END 3 */
  /* USER CODE END WHILE */
}
/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}
/* USER CODE BEGIN 4 */
/* USER CODE END 4 */
/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
