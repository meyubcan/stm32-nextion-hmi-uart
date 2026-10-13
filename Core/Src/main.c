/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  * @author         : M. Eyub Can, TECH-OHM (https://tech-ohm.com)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct { int sic; int kuv; int hiz; } preset_t;  /* one set of temperature/force/speed */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Value ranges */
#define SIC_MIN 50
#define SIC_MAX 250
#define KUV_MIN 0
#define KUV_MAX 200
#define HIZ_MIN 7
#define HIZ_MAX 15
/* Clock and ambient simulation */
#define CLK_STEP_MS   2000   /* change every this many ms */
#define CLK_MAX_TICKS 5      /* stop after this many changes */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
/* --- Nextion RX (collected byte by byte in the USART2 interrupt) --- */
uint8_t  rx_byte;            /* single-byte target for HAL_UART_Receive_IT */
uint8_t  rx_buf[64];         /* message payload (without the FF bytes) */
volatile uint16_t rx_len = 0;
uint8_t  ff_count = 0;       /* count of consecutive 0xFF (3 -> message complete) */
volatile uint8_t  msg_ready = 0;
char     msg[64];            /* complete message handed to the main loop */

/* --- Keypad and keyboard state --- */
int      va_mode = 0;        /* active mode: 0/1/2 home · 10-12 medical · 20-22 tyvek */
char     kp_buf[16];         /* digits or letters entered so far */
uint8_t  kp_len = 0;

/* --- Values --- */
preset_t pr_med = {0, 0, 0}; /* medical preset (kept) */
preset_t pr_tyv = {0, 0, 0}; /* tyvek preset (kept) */
int  act_sic = 0, act_kuv = 0, act_hiz = 0;  /* ACTIVE values shown on home */
int  va_paper = 2;           /* 0 medical · 1 tyvek · 2 none */

/* --- Bag counters --- */
int  cnt_med = 0, cnt_tyv = 0;

/* --- Current page, clock and ambient --- */
int  cur_page = 0;           /* 1 home · 2 keypad · 4 settings · 5 qr · 6 report */
int  clk_h = 14, clk_m = 32; /* start time */
int  amb = 30;               /* ambient temperature */
int  clk_ticks = 0;          /* how many times it has changed */
uint32_t clk_last = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
void nx_send(const char *cmd);      /* STM -> Nextion (adds 0xFF 0xFF 0xFF) */
void dbg(const char *s);            /* STM -> PuTTY (USART3) log line */
void nx_handle(const char *cmd);    /* parse an incoming command */
void clock_tick(void);              /* clock and ambient simulation */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_USART3_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_UART_Receive_IT(&huart2, &rx_byte, 1);   /* start Nextion RX */
  HAL_Delay(200);
  nx_send("bkcmd=0");                           /* no ack/error bytes from Nextion, keeps RX clean */
  dbg("=== Nextion UART test started ===");
  dbg("Nextion USART2@9600 | Debug USART3@115200");
  clk_last = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (msg_ready) {
      char local[64];
      strcpy(local, msg);      /* copy the message that came from the interrupt */
      msg_ready = 0;           /* free the buffer */
      nx_handle(local);
    }
    clock_tick();              /* advance the clock and ambient simulation */
  }
  /* USER CODE END 3 */
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

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* STM -> Nextion: command plus the required 0xFF 0xFF 0xFF terminator */
void nx_send(const char *cmd)
{
  uint8_t end[3] = {0xFF, 0xFF, 0xFF};
  HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen(cmd), 100);
  HAL_UART_Transmit(&huart2, end, 3, 100);
}

/* STM -> PuTTY (USART3): debug line plus CRLF */
void dbg(const char *s)
{
  HAL_UART_Transmit(&huart3, (uint8_t *)s, strlen(s), 100);
  HAL_UART_Transmit(&huart3, (uint8_t *)"\r\n", 2, 100);
}

/* write date and time to the current page (t_dt exists on pages 1, 4, 5, 6) */
void push_datetime(void)
{
  if (cur_page==1 || cur_page==4 || cur_page==5 || cur_page==6) {
    char c[48];
    snprintf(c, sizeof(c), "t_dt.txt=\"03.07.2026 - %02d:%02d\"", clk_h, clk_m);
    nx_send(c);
  }
}

/* ambient temperature (t_amb exists only on home) */
void push_ambient(void)
{
  if (cur_page==1) {
    char c[32];
    snprintf(c, sizeof(c), "t_amb.txt=\"%d\xC2\xB0" "C\"", amb);   /* utf-8 °C */
    nx_send(c);
  }
}

/* write the ACTIVE values to home (n0/n1/n_spd) */
void home_push_values(void)
{
  char c[32];
  snprintf(c, sizeof(c), "n0.val=%d",   act_sic); nx_send(c);
  snprintf(c, sizeof(c), "n1.val=%d",   act_kuv); nx_send(c);
  snprintf(c, sizeof(c), "n_spd.val=%d", act_hiz); nx_send(c);
}

/* write the bag counters to home (n_pos = selected material, n_tot = total) */
void home_push_counts(void)
{
  char c[32];
  int cur = (va_paper==0) ? cnt_med : (va_paper==1) ? cnt_tyv : 0;
  snprintf(c, sizeof(c), "n_pos.val=%d", cur);               nx_send(c);
  snprintf(c, sizeof(c), "n_tot.val=%d", cnt_med + cnt_tyv); nx_send(c);
}

/* refresh everything when home opens */
void on_home_open(void)
{
  push_datetime();
  push_ambient();
  home_push_values();
  home_push_counts();
}

/* write the preset values when settings opens */
void on_settings_open(void)
{
  char c[32];
  push_datetime();
  snprintf(c, sizeof(c), "t_m_temp.txt=\"%d\"",  pr_med.sic); nx_send(c);
  snprintf(c, sizeof(c), "t_m_force.txt=\"%d\"", pr_med.kuv); nx_send(c);
  snprintf(c, sizeof(c), "t_m_spd.txt=\"%d\"",   pr_med.hiz); nx_send(c);
  snprintf(c, sizeof(c), "t_t_temp.txt=\"%d\"",  pr_tyv.sic); nx_send(c);
  snprintf(c, sizeof(c), "t_t_force.txt=\"%d\"", pr_tyv.kuv); nx_send(c);
  snprintf(c, sizeof(c), "t_t_spd.txt=\"%d\"",   pr_tyv.hiz); nx_send(c);
}

/* is this a numeric field mode? (0/1/2 home · 10-12 medical · 20-22 tyvek) */
static int is_field_mode(int mode)
{
  return (mode % 10 <= 2) && (mode < 3 || mode >= 10);
}

/* write title and range when the keypad opens, depending on the mode */
void keypad_open(int mode)
{
  char c[64];
  if (is_field_mode(mode)) {                      /* numeric: temperature/force/speed */
    int f = mode % 10;
    const char *fld = (f==0) ? "Temperature" : (f==1) ? "Force"  : "Belt speed";
    const char *rng = (f==0) ? "50 - 250" : (f==1) ? "0 - 200" : "7 - 15";
    const char *pfx = (mode>=10 && mode<20) ? "Medical " : (mode>=20) ? "Tyvek " : "";
    snprintf(c, sizeof(c), "t_title.txt=\"%s%s\"", pfx, fld); nx_send(c);
    nx_send("t_val.txt=\"\"");
    nx_send("t_range.pco=44210");                 /* gray (normal) */
    snprintf(c, sizeof(c), "t_range.txt=\"Range %s\"", rng); nx_send(c);
    return;
  }
  /* non-numeric modes (4 password · 5 expiry · 6 date · 7 time), not handled yet */
  const char *title, *hint;
  switch (mode) {
    case 4:  title = "Password";        hint = "4-digit password";      break;
    case 5:  title = "Expiry"; hint = "+3mo / +6mo / off"; break;
    case 6:  title = "Date";        hint = "DD.MM.YYYY";          break;
    case 7:  title = "Time";         hint = "SS:DD";               break;
    default: title = "Input";        hint = "";                    break;
  }
  snprintf(c, sizeof(c), "t_title.txt=\"%s\"", title); nx_send(c);
  nx_send("t_val.txt=\"\"");
  nx_send("t_range.pco=44210");
  snprintf(c, sizeof(c), "t_range.txt=\"%s\"", hint); nx_send(c);
}

/* is the value within the range of this field type? */
int in_range(int mode, int v)
{
  int f = mode % 10;
  if (f==0) return (v>=SIC_MIN && v<=SIC_MAX);
  if (f==1) return (v>=KUV_MIN && v<=KUV_MAX);
  return (v>=HIZ_MIN && v<=HIZ_MAX);
}

/* keypad confirm (K:E): validate, then write to the target (active or preset) */
void keypad_confirm(void)
{
  int v = atoi(kp_buf);

  if (is_field_mode(va_mode)) {                  /* numeric field: temperature/force/speed */
    int f = va_mode % 10;
    if (!in_range(va_mode, v)) {                 /* out of range -> reject */
      const char *rng = (f==0) ? "50-250" : (f==1) ? "0-200" : "7-15";
      char c[64];
      nx_send("t_range.pco=63488");              /* red */
      snprintf(c, sizeof(c), "t_range.txt=\"Invalid, use %s\"", rng);
      nx_send(c);
      dbg("KEYPAD: out-of-range value rejected");
      kp_len = 0; kp_buf[0] = '\0';
      return;
    }
    if (va_mode <= 2) {                          /* HOME active value */
      if (f==0) act_sic=v; else if (f==1) act_kuv=v; else act_hiz=v;
      home_push_values();
      dbg("KEYPAD: active value updated");
    } else if (va_mode < 20) {                   /* MEDICAL preset (10-12) */
      if (f==0) pr_med.sic=v; else if (f==1) pr_med.kuv=v; else pr_med.hiz=v;
      if (va_paper==0) { if (f==0) act_sic=v; else if (f==1) act_kuv=v; else act_hiz=v; }
      dbg("SETTINGS: medical preset saved");
    } else {                                     /* TYVEK preset (20-22) */
      if (f==0) pr_tyv.sic=v; else if (f==1) pr_tyv.kuv=v; else pr_tyv.hiz=v;
      if (va_paper==1) { if (f==0) act_sic=v; else if (f==1) act_kuv=v; else act_hiz=v; }
      dbg("SETTINGS: tyvek preset saved");
    }
    kp_len = 0; kp_buf[0] = '\0';
    return;
  }

  /* non-numeric modes (4 password · 6 date · 7 time), next stage */
  {
    char c[64];
    snprintf(c, sizeof(c), "KEYPAD mode %d confirm: '%s' (not handled yet)", va_mode, kp_buf);
    dbg(c);
  }
  kp_len = 0; kp_buf[0] = '\0';
}

/* keypad keys (K:x) */
void handle_keypad_key(char k)
{
  char c[48];
  if (k>='0' && k<='9') {
    if (kp_len < sizeof(kp_buf)-1) { kp_buf[kp_len++]=k; kp_buf[kp_len]='\0'; }
    snprintf(c, sizeof(c), "t_val.txt=\"%s\"", kp_buf); nx_send(c);
  } else if (k=='B') {                           /* backspace */
    if (kp_len>0) { kp_len--; kp_buf[kp_len]='\0'; }
    snprintf(c, sizeof(c), "t_val.txt=\"%s\"", kp_buf); nx_send(c);
  } else if (k=='C') {                           /* cancel */
    kp_len=0; kp_buf[0]='\0';
  } else if (k=='E') {                           /* confirm */
    keypad_confirm();
  }
}

/* select or deselect a material (MTYP). Only one can be selected, STM keeps va_paper in sync. */
void paper_select(int paper)                     /* paper: 0 medical · 1 tyvek */
{
  if (va_paper == paper) {                        /* already selected -> deselect, values stay as they are */
    va_paper = 2;
    nx_send("va_paper=2");
    nx_send("bt_med.val=0");
    nx_send("bt_tyv.val=0");
    dbg(paper==0 ? "MEDICAL DESELECTED (values kept)"
                 : "TYVEK DESELECTED (values kept)");
  } else {                                        /* select -> load the preset into the active values */
    preset_t *p = (paper==0) ? &pr_med : &pr_tyv;
    va_paper = paper;
    nx_send(paper==0 ? "va_paper=0"   : "va_paper=1");
    nx_send(paper==0 ? "bt_med.val=1" : "bt_tyv.val=1");
    nx_send(paper==0 ? "bt_tyv.val=0" : "bt_med.val=0");
    act_sic = p->sic; act_kuv = p->kuv; act_hiz = p->hiz;
    home_push_values();
    home_push_counts();
    dbg(paper==0 ? "MEDICAL SELECTED (preset -> active)"
                 : "TYVEK SELECTED (preset -> active)");
  }
}

/* clock and ambient simulation: once every CLK_STEP_MS, CLK_MAX_TICKS times */
void clock_tick(void)
{
  uint32_t now = HAL_GetTick();
  if (clk_ticks >= CLK_MAX_TICKS) return;
  if (now - clk_last < CLK_STEP_MS) return;
  clk_last = now;
  clk_ticks++;
  clk_m++;
  if (clk_m >= 60) { clk_m = 0; clk_h = (clk_h + 1) % 24; }
  amb++;
  push_datetime();
  push_ambient();
}

/* ================== INCOMING COMMAND PARSER ==================
   Every command is logged to PuTTY first, then handled. */
void nx_handle(const char *cmd)
{
  char line[80];
  snprintf(line, sizeof(line), "RX: %s", cmd);
  dbg(line);

  /* --- Page opens --- */
  if (strcmp(cmd, "HOME") == 0)          { cur_page = 1; on_home_open(); }
  else if (strcmp(cmd, "SETOPEN") == 0)  { cur_page = 4; on_settings_open(); }
  else if (strcmp(cmd, "REPOPEN") == 0)  { cur_page = 6; push_datetime(); }
  else if (strncmp(cmd, "KPOPEN:", 7) == 0) {
    cur_page = 2;
    va_mode = atoi(cmd + 7);
    kp_len = 0; kp_buf[0] = '\0';
    keypad_open(va_mode);
  }
  /* --- Belt (bag counter goes up on BF:1) --- */
  else if (strcmp(cmd, "BF:1") == 0) {
    dbg("BELT FORWARD PRESSED");
    if      (va_paper==0) { cnt_med++; home_push_counts(); dbg("BAG +1 (medical)"); }
    else if (va_paper==1) { cnt_tyv++; home_push_counts(); dbg("BAG +1 (tyvek)"); }
    else                  { dbg("BAG NOT COUNTED (no material selected)"); }
  }
  else if (strcmp(cmd, "BF:0") == 0)     { dbg("BELT FORWARD RELEASED"); }
  else if (strcmp(cmd, "BB:1") == 0)     { dbg("BELT REVERSE PRESSED"); }
  else if (strcmp(cmd, "BB:0") == 0)     { dbg("BELT REVERSE RELEASED"); }
  /* --- Material selection --- */
  else if (strcmp(cmd, "MTYP:0") == 0)   { paper_select(0); }
  else if (strcmp(cmd, "MTYP:1") == 0)   { paper_select(1); }
  /* --- Keypad keys --- */
  else if (strncmp(cmd, "K:", 2) == 0)   { handle_keypad_key(cmd[2]); }
  /* --- SKT (expiry date) --- */
  else if (strcmp(cmd, "SKT:3") == 0)    { dbg("EXPIRY: +3 MONTHS"); }
  else if (strcmp(cmd, "SKT:6") == 0)    { dbg("EXPIRY: +6 MONTHS"); }
  else if (strcmp(cmd, "SKT:0") == 0)    { dbg("EXPIRY: OFF"); }
  /* --- Language and toggles --- */
  else if (strcmp(cmd, "LANG:0") == 0)   { dbg("LANGUAGE: TR"); }
  else if (strcmp(cmd, "LANG:1") == 0)   { dbg("LANGUAGE: EN"); }
  else if (strcmp(cmd, "PRN:1") == 0)    { dbg("PRINTER: ON"); }
  else if (strcmp(cmd, "PRN:0") == 0)    { dbg("PRINTER: OFF"); }
  else if (strcmp(cmd, "FNT:1") == 0)    { dbg("FONT: ON"); }
  else if (strcmp(cmd, "FNT:0") == 0)    { dbg("FONT: OFF"); }
  /* --- Keyboard (operator/lot): secondary, only buffered and logged --- */
  else if (strncmp(cmd, "KB:", 3) == 0) {
    if (kp_len < sizeof(kp_buf)-1) { kp_buf[kp_len++]=cmd[3]; kp_buf[kp_len]='\0'; }
    char c[40]; snprintf(c, sizeof(c), "t_inp.txt=\"%s\"", kp_buf); nx_send(c);
  }
  else if (strcmp(cmd, "KBDEL") == 0) {
    if (kp_len>0) { kp_len--; kp_buf[kp_len]='\0'; }
    char c[40]; snprintf(c, sizeof(c), "t_inp.txt=\"%s\"", kp_buf); nx_send(c);
  }
  else if (strcmp(cmd, "KBCLR") == 0)    { kp_len=0; kp_buf[0]='\0'; nx_send("t_inp.txt=\"\""); }
  else if (strcmp(cmd, "KBOK") == 0) {
    char c[64]; snprintf(c, sizeof(c), "KEYBOARD CONFIRM: %s", kp_buf); dbg(c);
    kp_len=0; kp_buf[0]='\0';
  }
  /* unknown commands were already logged above as "RX: ..." */
}

/* USART2 RX interrupt: called for every byte, message is complete after three 0xFF */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2) {
    if (rx_byte == 0xFF) {
      ff_count++;
      if (ff_count >= 3) {              /* message complete */
        rx_buf[rx_len] = '\0';
        if (!msg_ready && rx_len > 0) { /* do not overwrite a message that is still waiting */
          strcpy(msg, (char *)rx_buf);
          msg_ready = 1;
        }
        rx_len = 0;
        ff_count = 0;
      }
    } else {
      ff_count = 0;
      if (rx_len < sizeof(rx_buf) - 1) {
        rx_buf[rx_len++] = rx_byte;
      } else {
        rx_len = 0;                     /* overflow guard */
      }
    }
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);  /* re-arm for the next byte */
  }
}

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
