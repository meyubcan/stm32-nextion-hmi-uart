/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
typedef struct { int sic; int kuv; int hiz; } preset_t;  /* sicaklik/kuvvet/hiz seti */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Deger araliklari (dokumandan) */
#define SIC_MIN 50
#define SIC_MAX 250
#define KUV_MIN 0
#define KUV_MAX 200
#define HIZ_MIN 7
#define HIZ_MAX 15
/* Saat/ortam simulasyonu */
#define CLK_STEP_MS   2000   /* kac ms'de bir degissin */
#define CLK_MAX_TICKS 5      /* kac kez degisip dursun */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
/* --- Nextion RX (USART2 interrupt ile byte-byte toplama) --- */
uint8_t  rx_byte;            /* HAL_UART_Receive_IT tek byte hedefi */
uint8_t  rx_buf[64];         /* gelen mesaj payload'i (FF'ler haric) */
volatile uint16_t rx_len = 0;
uint8_t  ff_count = 0;       /* ardisik 0xFF sayaci (3 -> mesaj tamam) */
volatile uint8_t  msg_ready = 0;
char     msg[64];            /* main loop'a devredilen tam mesaj */

/* --- Keypad/keybd durumu --- */
int      va_mode = 0;        /* aktif mod: 0/1/2 home · 10-12 medikal · 20-22 tyvek */
char     kp_buf[16];         /* girilen rakam/harf buffer'i */
uint8_t  kp_len = 0;

/* --- Deger modeli --- */
preset_t pr_med = {0, 0, 0}; /* medikal SET degerleri (kalici) */
preset_t pr_tyv = {0, 0, 0}; /* tyvek  SET degerleri (kalici) */
int  act_sic = 0, act_kuv = 0, act_hiz = 0;  /* AKTIF (home) degerler */
int  va_paper = 2;           /* 0 medikal · 1 tyvek · 2 hicbiri */

/* --- Poset sayaclari --- */
int  cnt_med = 0, cnt_tyv = 0;

/* --- Sayfa takibi + saat/ortam --- */
int  cur_page = 0;           /* 1 home · 2 keypad · 4 settings · 5 qr · 6 report */
int  clk_h = 14, clk_m = 32; /* baslangic saati */
int  amb = 30;               /* ortam sicakligi */
int  clk_ticks = 0;          /* kac kez degisti */
uint32_t clk_last = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
void nx_send(const char *cmd);      /* STM -> Nextion (sona 0xFF 0xFF 0xFF) */
void dbg(const char *s);            /* STM -> PuTTY (USART3) log satiri */
void nx_handle(const char *cmd);    /* gelen komutu parse et */
void clock_tick(void);              /* tarih/saat/ortam simulasyonu */
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
  HAL_UART_Receive_IT(&huart2, &rx_byte, 1);   /* Nextion RX'i baslat */
  HAL_Delay(200);
  nx_send("bkcmd=0");                           /* Nextion ack/error donmesin -> RX temiz */
  dbg("=== Nextion UART testi basladi ===");
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
      strcpy(local, msg);      /* interrupt'tan gelen mesaji kopyala */
      msg_ready = 0;           /* buffer'i serbest birak */
      nx_handle(local);
    }
    clock_tick();              /* tarih/saat/ortam simulasyonunu ilerlet */
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

/* STM -> Nextion: komut + zorunlu 0xFF 0xFF 0xFF sonlandirici */
void nx_send(const char *cmd)
{
  uint8_t end[3] = {0xFF, 0xFF, 0xFF};
  HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen(cmd), 100);
  HAL_UART_Transmit(&huart2, end, 3, 100);
}

/* STM -> PuTTY (USART3): debug satiri + CRLF */
void dbg(const char *s)
{
  HAL_UART_Transmit(&huart3, (uint8_t *)s, strlen(s), 100);
  HAL_UART_Transmit(&huart3, (uint8_t *)"\r\n", 2, 100);
}

/* mevcut sayfaya tarih/saat bas (t_dt sayfa 1,4,5,6'da var) */
void push_datetime(void)
{
  if (cur_page==1 || cur_page==4 || cur_page==5 || cur_page==6) {
    char c[48];
    snprintf(c, sizeof(c), "t_dt.txt=\"03.07.2026 - %02d:%02d\"", clk_h, clk_m);
    nx_send(c);
  }
}

/* ortam sicakligi (sadece home'da t_amb var) */
void push_ambient(void)
{
  if (cur_page==1) {
    char c[32];
    snprintf(c, sizeof(c), "t_amb.txt=\"%d\xC2\xB0" "C\"", amb);   /* utf-8 °C */
    nx_send(c);
  }
}

/* AKTIF degerleri home'a bas (n0/n1/n_spd) */
void home_push_values(void)
{
  char c[32];
  snprintf(c, sizeof(c), "n0.val=%d",   act_sic); nx_send(c);
  snprintf(c, sizeof(c), "n1.val=%d",   act_kuv); nx_send(c);
  snprintf(c, sizeof(c), "n_spd.val=%d", act_hiz); nx_send(c);
}

/* poset sayaclarini home'a bas (n_pos=secili kagit, n_tot=toplam) */
void home_push_counts(void)
{
  char c[32];
  int cur = (va_paper==0) ? cnt_med : (va_paper==1) ? cnt_tyv : 0;
  snprintf(c, sizeof(c), "n_pos.val=%d", cur);               nx_send(c);
  snprintf(c, sizeof(c), "n_tot.val=%d", cnt_med + cnt_tyv); nx_send(c);
}

/* home acildiginda tum icerigi tazele */
void on_home_open(void)
{
  push_datetime();
  push_ambient();
  home_push_values();
  home_push_counts();
}

/* settings acildiginda preset degerlerini bas */
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

/* sayisal alan modu mu? (0/1/2 home · 10-12 medikal · 20-22 tyvek) */
static int is_field_mode(int mode)
{
  return (mode % 10 <= 2) && (mode < 3 || mode >= 10);
}

/* keypad acilisinda baslik/aralik bas (mod'a gore) */
void keypad_open(int mode)
{
  char c[64];
  if (is_field_mode(mode)) {                      /* sayisal: sic/kuv/hiz */
    int f = mode % 10;
    const char *fld = (f==0) ? "Sicaklik" : (f==1) ? "Kuvvet"  : "Bant hizi";
    const char *rng = (f==0) ? "50 - 250" : (f==1) ? "0 - 200" : "7 - 15";
    const char *pfx = (mode>=10 && mode<20) ? "Medikal " : (mode>=20) ? "Tyvek " : "";
    snprintf(c, sizeof(c), "t_title.txt=\"%s%s\"", pfx, fld); nx_send(c);
    nx_send("t_val.txt=\"\"");
    nx_send("t_range.pco=44210");                 /* gri (normal) */
    snprintf(c, sizeof(c), "t_range.txt=\"Aralik %s\"", rng); nx_send(c);
    return;
  }
  /* sayisal olmayan modlar (4 sifre · 5 SKT · 6 tarih · 7 saat) - tam islem sonra */
  const char *title, *hint;
  switch (mode) {
    case 4:  title = "Sifre";        hint = "4 haneli sifre";      break;
    case 5:  title = "Son kullanma"; hint = "+3ay / +6ay / kapat"; break;
    case 6:  title = "Tarih";        hint = "GG.AA.YYYY";          break;
    case 7:  title = "Saat";         hint = "SS:DD";               break;
    default: title = "Giris";        hint = "";                    break;
  }
  snprintf(c, sizeof(c), "t_title.txt=\"%s\"", title); nx_send(c);
  nx_send("t_val.txt=\"\"");
  nx_send("t_range.pco=44210");
  snprintf(c, sizeof(c), "t_range.txt=\"%s\"", hint); nx_send(c);
}

/* deger, mod'un alan tipine gore aralikta mi? */
int in_range(int mode, int v)
{
  int f = mode % 10;
  if (f==0) return (v>=SIC_MIN && v<=SIC_MAX);
  if (f==1) return (v>=KUV_MIN && v<=KUV_MAX);
  return (v>=HIZ_MIN && v<=HIZ_MAX);
}

/* keypad onay (K:E): dogrula + hedefe yaz (aktif mi / preset mi) */
void keypad_confirm(void)
{
  int v = atoi(kp_buf);

  if (is_field_mode(va_mode)) {                  /* sayisal alan: sic/kuv/hiz */
    int f = va_mode % 10;
    if (!in_range(va_mode, v)) {                 /* aralik disi -> reddet */
      const char *rng = (f==0) ? "50-250" : (f==1) ? "0-200" : "7-15";
      char c[64];
      nx_send("t_range.pco=63488");              /* kirmizi */
      snprintf(c, sizeof(c), "t_range.txt=\"Gecersiz - %s arasi\"", rng);
      nx_send(c);
      dbg("KEYPAD: gecersiz deger reddedildi");
      kp_len = 0; kp_buf[0] = '\0';
      return;
    }
    if (va_mode <= 2) {                          /* HOME aktif (anlik) */
      if (f==0) act_sic=v; else if (f==1) act_kuv=v; else act_hiz=v;
      home_push_values();
      dbg("KEYPAD: aktif deger guncellendi (anlik)");
    } else if (va_mode < 20) {                   /* MEDIKAL preset (10-12) */
      if (f==0) pr_med.sic=v; else if (f==1) pr_med.kuv=v; else pr_med.hiz=v;
      if (va_paper==0) { if (f==0) act_sic=v; else if (f==1) act_kuv=v; else act_hiz=v; }
      dbg("SETTINGS: medikal preset kaydedildi");
    } else {                                     /* TYVEK preset (20-22) */
      if (f==0) pr_tyv.sic=v; else if (f==1) pr_tyv.kuv=v; else pr_tyv.hiz=v;
      if (va_paper==1) { if (f==0) act_sic=v; else if (f==1) act_kuv=v; else act_hiz=v; }
      dbg("SETTINGS: tyvek preset kaydedildi");
    }
    kp_len = 0; kp_buf[0] = '\0';
    return;
  }

  /* sayisal olmayan modlar (4 sifre · 6 tarih · 7 saat) - sonraki asama */
  {
    char c[64];
    snprintf(c, sizeof(c), "KEYPAD mod %d onay: '%s' (henuz islenmiyor)", va_mode, kp_buf);
    dbg(c);
  }
  kp_len = 0; kp_buf[0] = '\0';
}

/* keypad tuslari (K:x) */
void handle_keypad_key(char k)
{
  char c[48];
  if (k>='0' && k<='9') {
    if (kp_len < sizeof(kp_buf)-1) { kp_buf[kp_len++]=k; kp_buf[kp_len]='\0'; }
    snprintf(c, sizeof(c), "t_val.txt=\"%s\"", kp_buf); nx_send(c);
  } else if (k=='B') {                           /* backspace */
    if (kp_len>0) { kp_len--; kp_buf[kp_len]='\0'; }
    snprintf(c, sizeof(c), "t_val.txt=\"%s\"", kp_buf); nx_send(c);
  } else if (k=='C') {                           /* iptal */
    kp_len=0; kp_buf[0]='\0';
  } else if (k=='E') {                           /* onay */
    keypad_confirm();
  }
}

/* kagit sec/kapat (MTYP) - toggle + tekli secim, STM va_paper'i takip eder */
void paper_select(int paper)                     /* paper: 0 medikal · 1 tyvek */
{
  if (va_paper == paper) {                        /* zaten seciliydi -> KAPAT (dondur) */
    va_paper = 2;
    nx_send("va_paper=2");
    nx_send("bt_med.val=0");
    nx_send("bt_tyv.val=0");
    dbg(paper==0 ? "MEDIKAL KAPATILDI (deger donduruldu)"
                 : "TYVEK KAPATILDI (deger donduruldu)");
  } else {                                        /* SEC -> preset'i aktif'e yukle */
    preset_t *p = (paper==0) ? &pr_med : &pr_tyv;
    va_paper = paper;
    nx_send(paper==0 ? "va_paper=0"   : "va_paper=1");
    nx_send(paper==0 ? "bt_med.val=1" : "bt_tyv.val=1");
    nx_send(paper==0 ? "bt_tyv.val=0" : "bt_med.val=0");
    act_sic = p->sic; act_kuv = p->kuv; act_hiz = p->hiz;
    home_push_values();
    home_push_counts();
    dbg(paper==0 ? "MEDIKAL SECILDI (preset -> aktif)"
                 : "TYVEK SECILDI (preset -> aktif)");
  }
}

/* tarih/saat/ortam simulasyonu - CLK_STEP_MS'de bir, CLK_MAX_TICKS kez */
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

/* ================== GELEN KOMUT PARSER ==================
   Her komut once PuTTY'ye loglanir, sonra ilgili is yapilir. */
void nx_handle(const char *cmd)
{
  char line[80];
  snprintf(line, sizeof(line), "RX: %s", cmd);
  dbg(line);

  /* --- Sayfa acilislari --- */
  if (strcmp(cmd, "HOME") == 0)          { cur_page = 1; on_home_open(); }
  else if (strcmp(cmd, "SETOPEN") == 0)  { cur_page = 4; on_settings_open(); }
  else if (strcmp(cmd, "REPOPEN") == 0)  { cur_page = 6; push_datetime(); }
  else if (strncmp(cmd, "KPOPEN:", 7) == 0) {
    cur_page = 2;
    va_mode = atoi(cmd + 7);
    kp_len = 0; kp_buf[0] = '\0';
    keypad_open(va_mode);
  }
  /* --- Bant (poset sayaci BF:1'de artar) --- */
  else if (strcmp(cmd, "BF:1") == 0) {
    dbg("BANT ILERI BASILI");
    if      (va_paper==0) { cnt_med++; home_push_counts(); dbg("POSET +1 (medikal)"); }
    else if (va_paper==1) { cnt_tyv++; home_push_counts(); dbg("POSET +1 (tyvek)"); }
    else                  { dbg("POSET ARTMADI (kagit secili degil)"); }
  }
  else if (strcmp(cmd, "BF:0") == 0)     { dbg("BANT ILERI DURDU"); }
  else if (strcmp(cmd, "BB:1") == 0)     { dbg("BANT GERI BASILI"); }
  else if (strcmp(cmd, "BB:0") == 0)     { dbg("BANT GERI DURDU"); }
  /* --- Kagit secimi --- */
  else if (strcmp(cmd, "MTYP:0") == 0)   { paper_select(0); }
  else if (strcmp(cmd, "MTYP:1") == 0)   { paper_select(1); }
  /* --- Keypad tuslari --- */
  else if (strncmp(cmd, "K:", 2) == 0)   { handle_keypad_key(cmd[2]); }
  /* --- SKT (son kullanma) --- */
  else if (strcmp(cmd, "SKT:3") == 0)    { dbg("SON KULLANMA: +3 AY"); }
  else if (strcmp(cmd, "SKT:6") == 0)    { dbg("SON KULLANMA: +6 AY"); }
  else if (strcmp(cmd, "SKT:0") == 0)    { dbg("SON KULLANMA: KAPAT"); }
  /* --- Dil / toggle'lar --- */
  else if (strcmp(cmd, "LANG:0") == 0)   { dbg("DIL: TR"); }
  else if (strcmp(cmd, "LANG:1") == 0)   { dbg("DIL: EN"); }
  else if (strcmp(cmd, "PRN:1") == 0)    { dbg("YAZICI: ACIK"); }
  else if (strcmp(cmd, "PRN:0") == 0)    { dbg("YAZICI: KAPALI"); }
  else if (strcmp(cmd, "FNT:1") == 0)    { dbg("FONT: ACIK"); }
  else if (strcmp(cmd, "FNT:0") == 0)    { dbg("FONT: KAPALI"); }
  /* --- Keybd (operator/lot) - ikincil, sadece buffer + log --- */
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
    char c[64]; snprintf(c, sizeof(c), "KEYBD ONAY: %s", kp_buf); dbg(c);
    kp_len=0; kp_buf[0]='\0';
  }
  /* taninmayan komutlar zaten yukarida "RX: ..." ile loglandi */
}

/* USART2 RX interrupt: her byte'ta cagrilir, 3x 0xFF gorunce mesaj tamam */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2) {
    if (rx_byte == 0xFF) {
      ff_count++;
      if (ff_count >= 3) {              /* mesaj tamamlandi */
        rx_buf[rx_len] = '\0';
        if (!msg_ready && rx_len > 0) { /* onceki islenmediyse ustune yazma */
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
        rx_len = 0;                     /* tasma korumasi */
      }
    }
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);  /* bir sonraki byte icin re-arm */
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
