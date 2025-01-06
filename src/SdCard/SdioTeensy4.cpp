/**
 * Copyright (c) 2011-2021 Bill Greiman
 * This file is part of the SdFat library for SD memory cards.
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#if defined(__IMXRT1062__)
#include "SdioTeensy.h"
#include "SdCardInfo.h"
#include "SdioCard.h"


//==============================================================================
// limit of K66 due to errata KINETIS_K_0N65N.
const uint32_t MAX_BLKCNT = 0XFFFF;
//==============================================================================
#define SDHC_PROCTL_DTW_4BIT 0x01
const uint32_t FIFO_WML = 16;
const uint32_t CMD8_RETRIES = 3;
const uint32_t BUSY_TIMEOUT_MICROS = 1000000;
//==============================================================================
const uint32_t SDHC_IRQSTATEN_MASK =
               SDHC_IRQSTATEN_DMAESEN | SDHC_IRQSTATEN_AC12ESEN |
               SDHC_IRQSTATEN_DEBESEN | SDHC_IRQSTATEN_DCESEN |
               SDHC_IRQSTATEN_DTOESEN | SDHC_IRQSTATEN_CIESEN |
               SDHC_IRQSTATEN_CEBESEN | SDHC_IRQSTATEN_CCESEN |
               SDHC_IRQSTATEN_CTOESEN | SDHC_IRQSTATEN_DINTSEN |
               SDHC_IRQSTATEN_TCSEN | SDHC_IRQSTATEN_CCSEN;

const uint32_t SDHC_IRQSTAT_CMD_ERROR =
               SDHC_IRQSTAT_CIE | SDHC_IRQSTAT_CEBE |
               SDHC_IRQSTAT_CCE | SDHC_IRQSTAT_CTOE;

const uint32_t SDHC_IRQSTAT_DATA_ERROR =
               SDHC_IRQSTAT_AC12E | SDHC_IRQSTAT_DEBE |
               SDHC_IRQSTAT_DCE | SDHC_IRQSTAT_DTOE;

const uint32_t SDHC_IRQSTAT_ERROR =
               SDHC_IRQSTAT_DMAE | SDHC_IRQSTAT_CMD_ERROR |
               SDHC_IRQSTAT_DATA_ERROR;

const uint32_t SDHC_IRQSIGEN_MASK =
               SDHC_IRQSIGEN_DMAEIEN | SDHC_IRQSIGEN_AC12EIEN |
               SDHC_IRQSIGEN_DEBEIEN | SDHC_IRQSIGEN_DCEIEN |
               SDHC_IRQSIGEN_DTOEIEN | SDHC_IRQSIGEN_CIEIEN |
               SDHC_IRQSIGEN_CEBEIEN | SDHC_IRQSIGEN_CCEIEN |
               SDHC_IRQSIGEN_CTOEIEN | SDHC_IRQSIGEN_TCIEN;
//==============================================================================
const uint32_t CMD_RESP_NONE = SDHC_XFERTYP_RSPTYP(0);

const uint32_t CMD_RESP_R1 = SDHC_XFERTYP_CICEN | SDHC_XFERTYP_CCCEN |
                             SDHC_XFERTYP_RSPTYP(2);

const uint32_t CMD_RESP_R1b = SDHC_XFERTYP_CICEN | SDHC_XFERTYP_CCCEN |
                              SDHC_XFERTYP_RSPTYP(3);

const uint32_t CMD_RESP_R2 = SDHC_XFERTYP_CCCEN | SDHC_XFERTYP_RSPTYP(1);

const uint32_t CMD_RESP_R3 = SDHC_XFERTYP_RSPTYP(2);

const uint32_t CMD_RESP_R6 = CMD_RESP_R1;

const uint32_t CMD_RESP_R7 = CMD_RESP_R1;

// Use low bits for m_psdhc->MIX_CTRL since bits 15-0 of m_psdhc->CMD_XFR_TYP are reserved.
const uint32_t SDHC_MIX_CTRL_MASK = SDHC_MIX_CTRL_DMAEN | SDHC_MIX_CTRL_BCEN |
                                    SDHC_MIX_CTRL_AC12EN |
                                    SDHC_MIX_CTRL_DDR_EN |
                                    SDHC_MIX_CTRL_DTDSEL |
                                    SDHC_MIX_CTRL_MSBSEL |
                                    SDHC_MIX_CTRL_NIBBLE_POS |
                                    SDHC_MIX_CTRL_AC23EN;

const uint32_t DATA_READ = SDHC_MIX_CTRL_DTDSEL | SDHC_XFERTYP_DPSEL;

const uint32_t DATA_READ_DMA = DATA_READ | SDHC_MIX_CTRL_DMAEN;

const uint32_t DATA_READ_MULTI_DMA = DATA_READ_DMA | SDHC_MIX_CTRL_MSBSEL |
                                     SDHC_MIX_CTRL_AC12EN | SDHC_MIX_CTRL_BCEN;

const uint32_t DATA_READ_MULTI_PGM = DATA_READ | SDHC_MIX_CTRL_MSBSEL;


const uint32_t DATA_WRITE_DMA = SDHC_XFERTYP_DPSEL | SDHC_MIX_CTRL_DMAEN;

const uint32_t DATA_WRITE_MULTI_DMA = DATA_WRITE_DMA | SDHC_MIX_CTRL_MSBSEL |
                                      SDHC_MIX_CTRL_AC12EN | SDHC_MIX_CTRL_BCEN;

const uint32_t DATA_WRITE_MULTI_PGM = SDHC_XFERTYP_DPSEL | SDHC_MIX_CTRL_MSBSEL;


const uint32_t ACMD6_XFERTYP = SDHC_XFERTYP_CMDINX(ACMD6) | CMD_RESP_R1;

const uint32_t ACMD41_XFERTYP = SDHC_XFERTYP_CMDINX(ACMD41) | CMD_RESP_R3;

const uint32_t CMD0_XFERTYP = SDHC_XFERTYP_CMDINX(CMD0) | CMD_RESP_NONE;

const uint32_t CMD2_XFERTYP = SDHC_XFERTYP_CMDINX(CMD2) | CMD_RESP_R2;

const uint32_t CMD3_XFERTYP = SDHC_XFERTYP_CMDINX(CMD3) | CMD_RESP_R6;

const uint32_t CMD6_XFERTYP = SDHC_XFERTYP_CMDINX(CMD6) | CMD_RESP_R1 |
                              DATA_READ_DMA;

const uint32_t CMD7_XFERTYP = SDHC_XFERTYP_CMDINX(CMD7) | CMD_RESP_R1b;

const uint32_t CMD8_XFERTYP = SDHC_XFERTYP_CMDINX(CMD8) | CMD_RESP_R7;

const uint32_t CMD9_XFERTYP = SDHC_XFERTYP_CMDINX(CMD9) | CMD_RESP_R2;

const uint32_t CMD10_XFERTYP = SDHC_XFERTYP_CMDINX(CMD10) | CMD_RESP_R2;

const uint32_t CMD11_XFERTYP = SDHC_XFERTYP_CMDINX(CMD11) | CMD_RESP_R1;

const uint32_t CMD12_XFERTYP = SDHC_XFERTYP_CMDINX(CMD12) | CMD_RESP_R1b |
                               SDHC_XFERTYP_CMDTYP(3);

const uint32_t CMD13_XFERTYP = SDHC_XFERTYP_CMDINX(CMD13) | CMD_RESP_R1;

const uint32_t CMD17_DMA_XFERTYP = SDHC_XFERTYP_CMDINX(CMD17) | CMD_RESP_R1 |
                                   DATA_READ_DMA;

const uint32_t CMD18_DMA_XFERTYP = SDHC_XFERTYP_CMDINX(CMD18) | CMD_RESP_R1 |
                                   DATA_READ_MULTI_DMA;

const uint32_t CMD18_PGM_XFERTYP = SDHC_XFERTYP_CMDINX(CMD18) | CMD_RESP_R1 |
                                   DATA_READ_MULTI_PGM;

const uint32_t CMD24_DMA_XFERTYP = SDHC_XFERTYP_CMDINX(CMD24) | CMD_RESP_R1 |
                                   DATA_WRITE_DMA;

const uint32_t CMD25_DMA_XFERTYP = SDHC_XFERTYP_CMDINX(CMD25) | CMD_RESP_R1 |
                                   DATA_WRITE_MULTI_DMA;

const uint32_t CMD25_PGM_XFERTYP = SDHC_XFERTYP_CMDINX(CMD25) | CMD_RESP_R1 |
                                   DATA_WRITE_MULTI_PGM;

const uint32_t CMD32_XFERTYP = SDHC_XFERTYP_CMDINX(CMD32) | CMD_RESP_R1;

const uint32_t CMD33_XFERTYP = SDHC_XFERTYP_CMDINX(CMD33) | CMD_RESP_R1;

const uint32_t CMD38_XFERTYP = SDHC_XFERTYP_CMDINX(CMD38) | CMD_RESP_R1b;

const uint32_t CMD55_XFERTYP = SDHC_XFERTYP_CMDINX(CMD55) | CMD_RESP_R1;

//==============================================================================
//------------------------------------------------------------------------------
//==============================================================================
SdioCard *SdioCard::s_pSdioCards[2] = {nullptr, nullptr};

#define DBG_TRACE Serial.print("TRACE."); Serial.println(__LINE__); delay(200);
#define USE_DEBUG_MODE 1
#if USE_DEBUG_MODE
#define DBG_IRQSTAT() if (m_psdhc->INT_STATUS) {Serial.print(__LINE__);\
        Serial.print(" IRQSTAT "); Serial.println(m_psdhc->INT_STATUS, HEX);}
void SdioCard::printRegs(uint32_t line) {
  uint32_t blkattr = m_psdhc->BLK_ATT;
  uint32_t xfertyp = m_psdhc->CMD_XFR_TYP;
  uint32_t prsstat = m_psdhc->PRES_STATE;
  uint32_t proctl = m_psdhc->PROT_CTRL;
  uint32_t irqstat = m_psdhc->INT_STATUS;
  Serial.print("\nLINE: ");
  Serial.println(line);
  Serial.print("BLKATTR ");
  Serial.println(blkattr, HEX);
  Serial.print("XFERTYP ");
  Serial.print(xfertyp, HEX);
  Serial.print(" CMD");
  Serial.print(xfertyp >> 24);
  Serial.print(" TYP");
  Serial.print((xfertyp >> 2) & 3);
  if (xfertyp & SDHC_XFERTYP_DPSEL) {Serial.print(" DPSEL");}
  Serial.println();
  Serial.print("PRSSTAT ");
  Serial.print(prsstat, HEX);
  if (prsstat & SDHC_PRSSTAT_BREN) {Serial.print(" BREN");}
  if (prsstat & SDHC_PRSSTAT_BWEN) {Serial.print(" BWEN");}
  if (prsstat & SDHC_PRSSTAT_RTA) {Serial.print(" RTA");}
  if (prsstat & SDHC_PRSSTAT_WTA) {Serial.print(" WTA");}
  if (prsstat & SDHC_PRSSTAT_SDOFF) {Serial.print(" SDOFF");}
  if (prsstat & SDHC_PRSSTAT_PEROFF) {Serial.print(" PEROFF");}
  if (prsstat & SDHC_PRSSTAT_HCKOFF) {Serial.print(" HCKOFF");}
  if (prsstat & SDHC_PRSSTAT_IPGOFF) {Serial.print(" IPGOFF");}
  if (prsstat & SDHC_PRSSTAT_SDSTB) {Serial.print(" SDSTB");}
  if (prsstat & SDHC_PRSSTAT_DLA) {Serial.print(" DLA");}
  if (prsstat & SDHC_PRSSTAT_CDIHB) {Serial.print(" CDIHB");}
  if (prsstat & SDHC_PRSSTAT_CIHB) {Serial.print(" CIHB");}
  Serial.println();
  Serial.print("PROCTL ");
  Serial.print(proctl, HEX);
  if (proctl & SDHC_PROCTL_SABGREQ) Serial.print(" SABGREQ");
  Serial.print(" EMODE");
  Serial.print((proctl >>4) & 3);
  Serial.print(" DWT");
  Serial.print((proctl >>1) & 3);
  Serial.println();
  Serial.print("IRQSTAT ");
  Serial.print(irqstat, HEX);
  if (irqstat & SDHC_IRQSTAT_BGE) {Serial.print(" BGE");}
  if (irqstat & SDHC_IRQSTAT_TC) {Serial.print(" TC");}
  if (irqstat & SDHC_IRQSTAT_CC) {Serial.print(" CC");}
  Serial.print("\nm_irqstat ");
  Serial.println(m_irqstat, HEX);
}
#else  // USE_DEBUG_MODE
#define DBG_IRQSTAT()
#endif  // USE_DEBUG_MODE
//==============================================================================
// Error function and macro.
#define sdError(code) setSdErrorCode(code, __LINE__)
inline bool SdioCard::setSdErrorCode(uint8_t code, uint32_t line) {
  m_errorCode = code;
  m_errorLine = line;
#if USE_DEBUG_MODE
  printRegs(line);
#endif  // USE_DEBUG_MODE
  return false;
}
//==============================================================================
// ISR
void SdioCard::sdIrs() {
  USDHC1_INT_SIGNAL_EN = 0;
  s_pSdioCards[0]->m_irqstat = USDHC1_INT_STATUS;
  USDHC1_INT_STATUS = s_pSdioCards[0]->m_irqstat;
  USDHC1_MIX_CTRL &= ~(SDHC_MIX_CTRL_AC23EN | SDHC_MIX_CTRL_DMAEN);
  s_pSdioCards[0]->m_dmaBusy = false;
}

void SdioCard::sdIrs2() {
  USDHC2_INT_SIGNAL_EN = 0;
  s_pSdioCards[1]->m_irqstat = USDHC2_INT_STATUS;
  USDHC2_INT_STATUS = s_pSdioCards[0]->m_irqstat;
  USDHC2_MIX_CTRL &= ~(SDHC_MIX_CTRL_AC23EN | SDHC_MIX_CTRL_DMAEN);
  s_pSdioCards[1]->m_dmaBusy = false;
}


//==============================================================================
// GPIO and clock functions.

//------------------------------------------------------------------------------
void SdioCard::gpioMux(uint8_t mode, bool fUseSDIO2) {
  if (!fUseSDIO2) {
    IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B0_04 = mode;  // DAT2
    IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B0_05 = mode;  // DAT3
    IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B0_00 = mode;  // CMD
    IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B0_01 = mode;  // CLK
    IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B0_02 = mode;  // DAT0
    IOMUXC_SW_MUX_CTL_PAD_GPIO_SD_B0_03 = mode;  // DAT1
  } else {
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_07 = mode; //USDHC2_DATA3
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_06 = mode; //USDHC2_DATA2
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_08 = mode; //USDHC2_CMD
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_09 = mode; //USDHC2_CLK
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_04 = mode; //USDHC2_DATA0
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_05 = mode; //USDHC2_DATA1
  }
}
//------------------------------------------------------------------------------
// add speed strength args?
void SdioCard::enableGPIO(bool enable, bool fUseSDIO2) {
  const uint32_t CLOCK_MASK = IOMUXC_SW_PAD_CTL_PAD_PKE |
                              IOMUXC_SW_PAD_CTL_PAD_DSE(7) |
                              IOMUXC_SW_PAD_CTL_PAD_SPEED(2);

  const uint32_t DATA_MASK = CLOCK_MASK | IOMUXC_SW_PAD_CTL_PAD_PUE |
                             IOMUXC_SW_PAD_CTL_PAD_PUS(1);
  if (enable) {
    gpioMux(fUseSDIO2? 6 : 0, fUseSDIO2);  // SDIO2 is on ALT6
    if (!fUseSDIO2) {
      IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B0_04 = DATA_MASK;   // DAT2
      IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B0_05 = DATA_MASK;   // DAT3
      IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B0_00 = DATA_MASK;   // CMD
      IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B0_01 = CLOCK_MASK;  // CLK
      IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B0_02 = DATA_MASK;   // DAT0
      IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B0_03 = DATA_MASK;   // DAT1
    } else {      
      IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_07 = DATA_MASK; //USDHC2_DATA3
      IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_06 = DATA_MASK; //USDHC2_DATA2
      IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_08 = DATA_MASK; //USDHC2_CMD
      IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_09 = CLOCK_MASK; //USDHC2_CLK
      IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_04 = DATA_MASK; //USDHC2_DATA0
      IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_05 = DATA_MASK; //USDHC2_DATA1

      // We need to set select input bits...
      IOMUXC_USDHC2_CLK_SELECT_INPUT = 0x1;
      IOMUXC_USDHC2_CMD_SELECT_INPUT = 0x01;
      IOMUXC_USDHC2_DATA0_SELECT_INPUT = 0x01;
      IOMUXC_USDHC2_DATA1_SELECT_INPUT = 0x01;
      IOMUXC_USDHC2_DATA2_SELECT_INPUT = 0x01;
      IOMUXC_USDHC2_DATA3_SELECT_INPUT = 0x01;

    }
  } else {
    gpioMux(5, fUseSDIO2);
  }
}
//------------------------------------------------------------------------------
void SdioCard::initClock(bool fUseSDIO2) {
  /* set PDF_528 PLL2PFD0 */
  CCM_ANALOG_PFD_528 |= (1 << 7);
  CCM_ANALOG_PFD_528 &= ~(0x3F << 0);
  CCM_ANALOG_PFD_528 |= ((24) & 0x3F << 0);  // 12 - 35
  CCM_ANALOG_PFD_528 &= ~(1 << 7);

  /* Enable USDHC clock. */
  if (!fUseSDIO2) {
    CCM_CCGR6 |= CCM_CCGR6_USDHC1(CCM_CCGR_ON);
    CCM_CSCDR1 &= ~(CCM_CSCDR1_USDHC1_PODF(7));
    CCM_CSCMR1 |= CCM_CSCMR1_USDHC1_CLK_SEL;          // PLL2PFD0
  //  CCM_CSCDR1 |= CCM_CSCDR1_USDHC1_CLK_PODF((7)); / &0x7  WHG
    CCM_CSCDR1 |= CCM_CSCDR1_USDHC1_CLK_PODF((1));
  } else {
    CCM_CCGR6 |= CCM_CCGR6_USDHC2(CCM_CCGR_ON);
    CCM_CSCDR1 &= ~(CCM_CSCDR1_USDHC2_PODF(7));
    CCM_CSCMR1 |= CCM_CSCMR1_USDHC2_CLK_SEL;          // PLL2PFD0
  //  CCM_CSCDR1 |= CCM_CSCDR1_USDHC2_CLK_PODF((7)); / &0x7  WHG
    CCM_CSCDR1 |= CCM_CSCDR1_USDHC2_PODF(1);
  }
}
//------------------------------------------------------------------------------
uint32_t SdioCard::baseClock() {
  uint32_t divider = ((CCM_CSCDR1 >> 11) & 0x7) + 1;
  return (528000000U * 3)/((CCM_ANALOG_PFD_528 & 0x3F)/6)/divider;
}
//==============================================================================
// Static functions.
bool SdioCard::cardAcmd(uint32_t rca, uint32_t xfertyp, uint32_t arg) {
  return cardCommand(CMD55_XFERTYP, rca) && cardCommand (xfertyp, arg);
}
//------------------------------------------------------------------------------
bool SdioCard::cardCommand(uint32_t xfertyp, uint32_t arg) {
  DBG_IRQSTAT();
  if (waitTimeout(&SdioCard::isBusyCommandInhibit)) {
    Serial.printf("cardCommand(%x, %x) error isBusyCommandInhibit)\n", xfertyp, arg);
    return false;  // Caller will set errorCode.
  }
  m_psdhc->CMD_ARG = arg;
  // Set MIX_CTRL if data transfer.
  if (xfertyp & SDHC_XFERTYP_DPSEL) {
    m_psdhc->MIX_CTRL &= ~SDHC_MIX_CTRL_MASK;
    m_psdhc->MIX_CTRL |= xfertyp & SDHC_MIX_CTRL_MASK;
  }
  xfertyp &= ~SDHC_MIX_CTRL_MASK;
  m_psdhc->CMD_XFR_TYP = xfertyp;
  if (waitTimeout(&SdioCard::isBusyCommandComplete)) {
    Serial.printf("cardCommand(%x, %x) error isBusyCommandComplete\n", xfertyp, arg);
    return false;  // Caller will set errorCode.
  }
  m_irqstat = m_psdhc->INT_STATUS;
  m_psdhc->INT_STATUS = m_irqstat;

  bool return_value = (m_irqstat & SDHC_IRQSTAT_CC) && !(m_irqstat & SDHC_IRQSTAT_CMD_ERROR);
  if (!return_value) Serial.printf("cardCommand(%x, %x) error return false", xfertyp, arg);  
  return return_value;      
}
//------------------------------------------------------------------------------
bool SdioCard::cardCMD6(uint32_t arg, uint8_t* status) {
  // CMD6 returns 64 bytes.
  if (waitTimeout(&SdioCard::isBusyCMD13)) {
    return sdError(SD_CARD_ERROR_CMD13);
  }
  enableDmaIrs();
  m_psdhc->DS_ADDR  = (uint32_t)status;
  m_psdhc->BLK_ATT = SDHC_BLKATTR_BLKCNT(1) | SDHC_BLKATTR_BLKSIZE(64);
  m_psdhc->INT_SIGNAL_EN = SDHC_IRQSIGEN_MASK;
  if (!cardCommand(CMD6_XFERTYP, arg)) {
    return sdError(SD_CARD_ERROR_CMD6);
  }
  if (!waitDmaStatus()) {
    return sdError(SD_CARD_ERROR_DMA);
  }
  return true;
}
//------------------------------------------------------------------------------
void SdioCard::enableDmaIrs() {
  m_dmaBusy = true;
  m_irqstat = 0;
}
//------------------------------------------------------------------------------
void SdioCard::initSDHC() {
  // initialize Hardware registers and this ointer
  bool fUseSDIO2 = (m_sdioConfig.options() & USE_SDIO2)? true : false;
  if (fUseSDIO2) {
    s_pSdioCards[1] = this; 
    m_psdhc = (IMXRT_USDHC_t*)IMXRT_USDHC2_ADDRESS;
  } else {
    s_pSdioCards[0] = this; 
    m_psdhc = (IMXRT_USDHC_t*)IMXRT_USDHC1_ADDRESS;    
  }

  initClock(fUseSDIO2);

  // Disable GPIO clock.
  enableGPIO(false, fUseSDIO2);

  m_psdhc->MIX_CTRL |= 0x80000000;

  // Reset SDHC. Use default Water Mark Level of 16.
  m_psdhc->SYS_CTRL |= SDHC_SYSCTL_RSTA | SDHC_SYSCTL_SDCLKFS(0x80);

  while (m_psdhc->SYS_CTRL & SDHC_SYSCTL_RSTA) {
  }

  // Set initial SCK rate.
  setSdclk(SD_MAX_INIT_RATE_KHZ);

  enableGPIO(true, fUseSDIO2);

  // Enable desired IRQSTAT bits.
  m_psdhc->INT_STATUS_EN = SDHC_IRQSTATEN_MASK;

  if (!fUseSDIO2) {
    attachInterruptVector(IRQ_SDHC1, sdIrs);
    NVIC_SET_PRIORITY(IRQ_SDHC1, 6*16);
    NVIC_ENABLE_IRQ(IRQ_SDHC1);
  } else {
    attachInterruptVector(IRQ_SDHC2, sdIrs2);
    NVIC_SET_PRIORITY(IRQ_SDHC2, 6*16);
    NVIC_ENABLE_IRQ(IRQ_SDHC2);    
  }
  // Send 80 clocks to card.
  m_psdhc->SYS_CTRL |= SDHC_SYSCTL_INITA;
  while (m_psdhc->SYS_CTRL & SDHC_SYSCTL_INITA) {
  }
}
//------------------------------------------------------------------------------
uint32_t SdioCard::statusCMD13() {
  return cardCommand(CMD13_XFERTYP, m_rca) ? m_psdhc->CMD_RSP0 : 0;
}
//------------------------------------------------------------------------------
bool SdioCard::isBusyCMD13() {
  return !(statusCMD13() & CARD_STATUS_READY_FOR_DATA);
}
//------------------------------------------------------------------------------
bool SdioCard::isBusyCommandComplete() {
  return !(m_psdhc->INT_STATUS & (SDHC_IRQSTAT_CC | SDHC_IRQSTAT_CMD_ERROR));
}
//------------------------------------------------------------------------------
bool SdioCard::isBusyCommandInhibit() {
  return m_psdhc->PRES_STATE & SDHC_PRSSTAT_CIHB;
}
//------------------------------------------------------------------------------
bool SdioCard::isBusyDat() {
  return m_psdhc->PRES_STATE & (1 << 24) ? false : true;
}
//------------------------------------------------------------------------------
bool SdioCard::isBusyDMA() {
  return m_dmaBusy;
}
//------------------------------------------------------------------------------
bool SdioCard::isBusyFifoRead() {
  return !(m_psdhc->PRES_STATE & SDHC_PRSSTAT_BREN);
}
//------------------------------------------------------------------------------
bool SdioCard::isBusyFifoWrite() {
  return !(m_psdhc->PRES_STATE & SDHC_PRSSTAT_BWEN);
}
//------------------------------------------------------------------------------
bool SdioCard::isBusyTransferComplete() {
  return !(m_psdhc->INT_STATUS & (SDHC_IRQSTAT_TC | SDHC_IRQSTAT_ERROR));
}
//------------------------------------------------------------------------------
bool SdioCard::rdWrSectors(uint32_t xfertyp,
                       uint32_t sector, uint8_t* buf, size_t n) {
  if ((3 & (uint32_t)buf) || n == 0) {
    return sdError(SD_CARD_ERROR_DMA);
  }
  if (yieldTimeout(&SdioCard::isBusyCMD13)) {
    return sdError(SD_CARD_ERROR_CMD13);
  }
  enableDmaIrs();
  m_psdhc->DS_ADDR  = (uint32_t)buf;
  m_psdhc->BLK_ATT = SDHC_BLKATTR_BLKCNT(n) | SDHC_BLKATTR_BLKSIZE(512);
  m_psdhc->INT_SIGNAL_EN = SDHC_IRQSIGEN_MASK;
  if (!cardCommand(xfertyp, m_highCapacity ? sector : 512*sector)) {
    return false;
  }
  return waitDmaStatus();
}
//------------------------------------------------------------------------------
// Read 16 byte CID or CSD register.
bool SdioCard::readReg16(uint32_t xfertyp, void* data) {
  uint8_t* d = reinterpret_cast<uint8_t*>(data);
  if (!cardCommand(xfertyp, m_rca)) {
    return false;  // Caller will set errorCode.
  }
  uint32_t sr[] = {m_psdhc->CMD_RSP0, m_psdhc->CMD_RSP1, m_psdhc->CMD_RSP2, m_psdhc->CMD_RSP3};
  for (int i = 0; i < 15; i++) {
    d[14 - i] = sr[i/4] >> 8*(i%4);
  }
  d[15] = 0;
  return true;
}
//------------------------------------------------------------------------------
void SdioCard::setSdclk(uint32_t kHzMax) {
  const uint32_t DVS_LIMIT = 0X10;
  const uint32_t SDCLKFS_LIMIT = 0X100;
  uint32_t dvs = 1;
  uint32_t sdclkfs = 1;
  uint32_t maxSdclk = 1000*kHzMax;
  uint32_t base = baseClock();

  while ((base/(sdclkfs*DVS_LIMIT) > maxSdclk) && (sdclkfs < SDCLKFS_LIMIT)) {
    sdclkfs <<= 1;
  }
  while ((base/(sdclkfs*dvs) > maxSdclk) && (dvs < DVS_LIMIT)) {
    dvs++;
  }
  m_sdClkKhz = base/(1000*sdclkfs*dvs);
  sdclkfs >>= 1;
  dvs--;

  // Change dividers.
  uint32_t sysctl = m_psdhc->SYS_CTRL & ~(SDHC_SYSCTL_DTOCV_MASK
                    | SDHC_SYSCTL_DVS_MASK | SDHC_SYSCTL_SDCLKFS_MASK);

  m_psdhc->SYS_CTRL = sysctl | SDHC_SYSCTL_DTOCV(0x0E) | SDHC_SYSCTL_DVS(dvs)
                | SDHC_SYSCTL_SDCLKFS(sdclkfs);

  // Wait until the SDHC clock is stable.
  while (!(m_psdhc->PRES_STATE & SDHC_PRSSTAT_SDSTB)) {
  }

}
//------------------------------------------------------------------------------
bool SdioCard::transferStop() {
  // This fix allows CDIHB to be cleared in Tennsy 3.x without a reset.
  m_psdhc->PROT_CTRL &= ~SDHC_PROCTL_SABGREQ;
  if (!cardCommand(CMD12_XFERTYP, 0)) {
    return sdError(SD_CARD_ERROR_CMD12);
  }
  if (yieldTimeout(&SdioCard::isBusyDat)) {
    return sdError(SD_CARD_ERROR_CMD13);
  }
  if (m_psdhc->PRES_STATE & SDHC_PRSSTAT_CDIHB) {
    // This should not happen after above fix.
    // Save registers before reset DAT lines.
    uint32_t irqsststen = m_psdhc->INT_STATUS_EN;
    uint32_t proctl = m_psdhc->PROT_CTRL & ~SDHC_PROCTL_SABGREQ;
    // Do reset to clear CDIHB.  Should be a better way!
    m_psdhc->SYS_CTRL |= SDHC_SYSCTL_RSTD;
    // Restore registers.
    m_psdhc->INT_STATUS_EN = irqsststen;
    m_psdhc->PROT_CTRL = proctl;
  }
  return true;
}
//------------------------------------------------------------------------------
// Return true if timeout occurs.
bool SdioCard::yieldTimeout(pcheckfcn fcn) {
  m_busyFcn = fcn;
  uint32_t m = micros();
  while ((this->*fcn)()) {
    if ((micros() - m) > BUSY_TIMEOUT_MICROS) {
      m_busyFcn = 0;
      return true;
    }
    yield();
  }
  m_busyFcn = 0;
  return false;  // Caller will set errorCode.
}
//------------------------------------------------------------------------------
bool SdioCard::waitDmaStatus() {
  if (yieldTimeout(&SdioCard::isBusyDMA)) {
    return false;  // Caller will set errorCode.
  }
  return (m_irqstat & SDHC_IRQSTAT_TC) && !(m_irqstat & SDHC_IRQSTAT_ERROR);
}
//------------------------------------------------------------------------------
// Return true if timeout occurs.
bool SdioCard::waitTimeout(pcheckfcn fcn) {
  uint32_t m = micros();
  while ((this->*fcn)()) {
    if ((micros() - m) > BUSY_TIMEOUT_MICROS) {
      return true;
    }
  }
  return false;  // Caller will set errorCode.
}
//------------------------------------------------------------------------------
bool SdioCard::waitTransferComplete() {
  if (!m_transferActive) {
    return true;
  }
  uint32_t m = micros();
  bool timeOut = false;
  while (isBusyTransferComplete()) {
    if ((micros() - m) > BUSY_TIMEOUT_MICROS) {
      timeOut = true;
      break;
    }
  }
  m_transferActive = false;
  m_irqstat = m_psdhc->INT_STATUS;
  m_psdhc->INT_STATUS = m_irqstat;
  if (timeOut || (m_irqstat & SDHC_IRQSTAT_ERROR)) {
    return sdError(SD_CARD_ERROR_TRANSFER_COMPLETE);
  }
  return true;
}
//==============================================================================
// Start of SdioCard member functions.
//==============================================================================
bool SdioCard::begin(SdioConfig sdioConfig) {
  uint32_t kHzSdClk;
  uint32_t arg;
  m_sdioConfig = sdioConfig;
  m_curState = IDLE_STATE;
  m_initDone = false;
  m_errorCode = SD_CARD_ERROR_NONE;
  m_highCapacity = false;
  m_version2 = false;
  bool fUseSDIO2 = (m_sdioConfig.options() & USE_SDIO2)? true : false;
  Serial.printf("SdioCard::begin: %u\n", fUseSDIO2);

  // initialize controller.
  initSDHC();
  if (!cardCommand(CMD0_XFERTYP, 0)) {
    Serial.println("cardCommand(CMD0_XFERTYP,0) -- failed");
    return sdError(SD_CARD_ERROR_CMD0);
  }
  // Try several times for case of reset delay.
  for (uint32_t i = 0; i < CMD8_RETRIES; i++) {
    if (cardCommand(CMD8_XFERTYP, 0X1AA)) {
      if (m_psdhc->CMD_RSP0 != 0X1AA) {
        return sdError(SD_CARD_ERROR_CMD8);
      }
      m_version2 = true;
      break;
    }
  }
  // Old version 1 cards have trouble with Teensy 4.1 after CMD8.
  // For reasons unknown, SDIO stops working after the cards does
  // not reply.  Simply restarting and CMD0 is a crude workaround.
  if (!m_version2) {
    initSDHC();
    cardCommand(CMD0_XFERTYP, 0);
  }
  arg = m_version2 ? 0X40300000 : 0x00300000;
  int m = micros();
  do {
    if (!cardAcmd(0, ACMD41_XFERTYP, arg) ||
       ((micros() - m) > BUSY_TIMEOUT_MICROS)) {
      return sdError(SD_CARD_ERROR_ACMD41);
    }
  } while ((m_psdhc->CMD_RSP0 & 0x80000000) == 0);
  m_ocr = m_psdhc->CMD_RSP0;
  if (m_psdhc->CMD_RSP0 & 0x40000000) {
    // Is high capacity.
    m_highCapacity = true;
  }
  if (!cardCommand(CMD2_XFERTYP, 0)) {
    return sdError(SD_CARD_ERROR_CMD2);
  }
  if (!cardCommand(CMD3_XFERTYP, 0)) {
    return sdError(SD_CARD_ERROR_CMD3);
  }
  m_rca = m_psdhc->CMD_RSP0 & 0xFFFF0000;

  if (!readReg16(CMD9_XFERTYP, &m_csd)) {
    return sdError(SD_CARD_ERROR_CMD9);
  }
  if (!readReg16(CMD10_XFERTYP, &m_cid)) {
    return sdError(SD_CARD_ERROR_CMD10);
  }
  if (!cardCommand(CMD7_XFERTYP, m_rca)) {
    return sdError(SD_CARD_ERROR_CMD7);
  }
  // Set card to bus width four.
  if (!cardAcmd(m_rca, ACMD6_XFERTYP, 2)) {
    return sdError(SD_CARD_ERROR_ACMD6);
  }
  // Set SDHC to bus width four.
  m_psdhc->PROT_CTRL &= ~SDHC_PROCTL_DTW_MASK;
  m_psdhc->PROT_CTRL |= SDHC_PROCTL_DTW(SDHC_PROCTL_DTW_4BIT);

  m_psdhc->WTMK_LVL = SDHC_WML_RDWML(FIFO_WML) | SDHC_WML_WRWML(FIFO_WML);

  // Determine if High Speed mode is supported and set frequency.
  // Check status[16] for error 0XF or status[16] for new mode 0X1.
  uint8_t status[64];
  // Thanks to tompilot :-)
  // https://forum.pjrc.com/threads/69460?p=331762&viewfull=1#post331762
  // Ask if highspeed mode supported. Returns true if SD card reacts to the command within timeout.
  bool highSpeedModeAsk = cardCMD6(0X00FFFFFF, status);
  // Check the SD-card's response. This bit must be set.
  bool highspeedModeSupported = (2 & status[13]);
  // safely ask for a switch request and accept in this case if there is no response.
  kHzSdClk = 25000;
  if (highSpeedModeAsk && highspeedModeSupported) {
    uint8_t err_code_before = m_errorCode;
    uint32_t m_errorLine_before = m_errorLine;
    // Switch to highspeed mode request. Returns true if SD card reacts within timeout.
    bool switchRequestAsk = cardCMD6(0X80FFFFF1, status);
    // Check the SD-card's response. This bit must be set.
    bool switchRequestDone = ((status[16] & 0XF) == 1);
    if (switchRequestAsk && switchRequestDone) {
      kHzSdClk = 50000;
    } else {
      // Maybe there are cards that say they support highspeed mode, but won't respond to
      // a switch request.  If it says that highspeed mode was supported, but the card did
      // not react to the switch request, we accept the timeout in this case and proceed
      // with non-highspeed mode.
      // We also need to revert to the error code from before, as cardCMD6(0X80FFFFF1, status)
      // will change the error code & line otherwise.
      m_errorCode = err_code_before;
      m_errorLine = m_errorLine_before;
    }
  }
  // Disable GPIO.
  enableGPIO(false, fUseSDIO2);

  // Set the SDHC SCK frequency.
  setSdclk(kHzSdClk);

  // Enable GPIO.
  enableGPIO(true, fUseSDIO2);
  m_initDone = true;
  return true;
}
//------------------------------------------------------------------------------
bool SdioCard::erase(uint32_t firstSector, uint32_t lastSector) {
#if ENABLE_TEENSY_SDIO_MOD
  if (m_curState != IDLE_STATE && !syncDevice()) {
    return false;
  }
#endif  // ENABLE_TEENSY_SDIO_MOD
  // check for single sector erase
  if (!m_csd.v1.erase_blk_en) {
    // erase size mask
    uint8_t m = (m_csd.v1.sector_size_high << 1) | m_csd.v1.sector_size_low;
    if ((firstSector & m) != 0 || ((lastSector + 1) & m) != 0) {
      // error card can't erase specified area
      return sdError(SD_CARD_ERROR_ERASE_SINGLE_SECTOR);
    }
  }
  if (!m_highCapacity) {
    firstSector <<= 9;
    lastSector <<= 9;
  }
  if (!cardCommand(CMD32_XFERTYP, firstSector)) {
    return sdError(SD_CARD_ERROR_CMD32);
  }
  if (!cardCommand(CMD33_XFERTYP, lastSector)) {
     return sdError(SD_CARD_ERROR_CMD33);
  }
  if (!cardCommand(CMD38_XFERTYP, 0)) {
    return sdError(SD_CARD_ERROR_CMD38);
  }
  if (waitTimeout(&SdioCard::isBusyCMD13)) {
    return sdError(SD_CARD_ERROR_ERASE_TIMEOUT);
  }
  return true;
}
//------------------------------------------------------------------------------
uint8_t SdioCard::errorCode() const {
  return m_errorCode;
}
//------------------------------------------------------------------------------
uint32_t SdioCard::errorData() const {
  return m_irqstat;
}
//------------------------------------------------------------------------------
uint32_t SdioCard::errorLine() const {
  return m_errorLine;
}
//------------------------------------------------------------------------------
bool SdioCard::isBusy() {
  if (m_sdioConfig.useDma()) {
    return m_busyFcn ? (this->*m_busyFcn)() : m_initDone && isBusyCMD13();
  } else {
    if (m_transferActive) {
      if (isBusyTransferComplete()) {
        return true;
      }
      return false;
    }
    // Use DAT0 low as busy.
    return m_psdhc->PRES_STATE & (1 << 24) ? false : true;
  }
}
//------------------------------------------------------------------------------
uint32_t SdioCard::kHzSdClk() {
  return m_sdClkKhz;
}
//------------------------------------------------------------------------------
bool SdioCard::readCID(cid_t* cid) {
  memcpy(cid, &m_cid, 16);
  return true;
}
//------------------------------------------------------------------------------
bool SdioCard::readCSD(csd_t* csd) {
  memcpy(csd, &m_csd, 16);
  return true;
}
//------------------------------------------------------------------------------
bool SdioCard::readData(uint8_t* dst) {
  DBG_IRQSTAT();
  uint32_t* p32 = reinterpret_cast<uint32_t*>(dst);

  if (!(m_psdhc->PRES_STATE & SDHC_PRSSTAT_RTA)) {
    m_psdhc->PROT_CTRL &= ~SDHC_PROCTL_SABGREQ;
    noInterrupts();
    m_psdhc->PROT_CTRL |= SDHC_PROCTL_CREQ;
    m_psdhc->PROT_CTRL |= SDHC_PROCTL_SABGREQ;
    interrupts();
  }
  if (waitTimeout(&SdioCard::isBusyFifoRead)) {
    return sdError(SD_CARD_ERROR_READ_FIFO);
  }
  for (uint32_t iw = 0 ; iw < 512/(4*FIFO_WML); iw++) {
    while (0 == (m_psdhc->PRES_STATE & SDHC_PRSSTAT_BREN)) {
    }
    for (uint32_t i = 0; i < FIFO_WML; i++) {
      p32[i] = m_psdhc->DATA_BUFF_ACC_PORT;
    }
    p32 += FIFO_WML;
  }
  if (waitTimeout(&SdioCard::isBusyTransferComplete)) {
    return sdError(SD_CARD_ERROR_READ_TIMEOUT);
  }
  m_irqstat = m_psdhc->INT_STATUS;
  m_psdhc->INT_STATUS = m_irqstat;
  return (m_irqstat & SDHC_IRQSTAT_TC) && !(m_irqstat & SDHC_IRQSTAT_ERROR);
}
//------------------------------------------------------------------------------
bool SdioCard::readOCR(uint32_t* ocr) {
  *ocr = m_ocr;
  return true;
}
//------------------------------------------------------------------------------
bool SdioCard::readSector(uint32_t sector, uint8_t* dst) {
  if (m_sdioConfig.useDma()) {
    uint8_t aligned[512];

    uint8_t* ptr = (uint32_t)dst & 3 ? aligned : dst;

    if (!rdWrSectors(CMD17_DMA_XFERTYP, sector, ptr, 1)) {
      return sdError(SD_CARD_ERROR_CMD17);
    }
    if (ptr != dst) {
      memcpy(dst, aligned, 512);
    }
  } else {
    if (!waitTransferComplete()) {
      return false;
    }
    if (m_curState != READ_STATE || sector != m_curSector) {
      if (!syncDevice()) {
        return false;
      }
      if (!readStart(sector)) {
        return false;
      }
      m_curSector = sector;
      m_curState = READ_STATE;
    }
    if (!readData(dst)) {
      return false;
    }
    m_curSector++;
  }
  return true;
}
//------------------------------------------------------------------------------
bool SdioCard::readSectors(uint32_t sector, uint8_t* dst, size_t n) {
  if (m_sdioConfig.useDma()) {
    if ((uint32_t)dst & 3) {
      for (size_t i = 0; i < n; i++, sector++, dst += 512) {
        if (!readSector(sector, dst)) {
          return false;  // readSector will set errorCode.
        }
      }
      return true;
    }
    if (!rdWrSectors(CMD18_DMA_XFERTYP, sector, dst, n)) {
      return sdError(SD_CARD_ERROR_CMD18);
    }
  } else {
    for (size_t i = 0; i < n; i++) {
      if (!readSector(sector + i, dst + i*512UL)) {
        return false;
      }
    }
  }
  return true;
}
//------------------------------------------------------------------------------
// SDHC will do Auto CMD12 after count sectors.
bool SdioCard::readStart(uint32_t sector) {
  DBG_IRQSTAT();
  if (yieldTimeout(&SdioCard::isBusyCMD13)) {
    return sdError(SD_CARD_ERROR_CMD13);
  }
  m_psdhc->PROT_CTRL |= SDHC_PROCTL_SABGREQ;
  // Infinite transfer.
  m_psdhc->BLK_ATT = SDHC_BLKATTR_BLKSIZE(512);

  if (!cardCommand(CMD18_PGM_XFERTYP, m_highCapacity ? sector : 512*sector)) {
    return sdError(SD_CARD_ERROR_CMD18);
  }
  return true;
}
//------------------------------------------------------------------------------
bool SdioCard::readStop() {
  return transferStop();
}
//------------------------------------------------------------------------------
uint32_t SdioCard::sectorCount() {
  return sdCardCapacity(&m_csd);
}
//------------------------------------------------------------------------------
uint32_t SdioCard::status() {
  return statusCMD13();
}
//------------------------------------------------------------------------------
bool SdioCard::stopTransmission(bool blocking) {
  m_curState = IDLE_STATE;
  // This fix allows CDIHB to be cleared in Tennsy 3.x without a reset.
  m_psdhc->PROT_CTRL &= ~SDHC_PROCTL_SABGREQ;
  if (!cardCommand(CMD12_XFERTYP, 0)) {
    return sdError(SD_CARD_ERROR_CMD12);
  }
  if (blocking) {
    if (yieldTimeout(&SdioCard::isBusyDat)) {
      return sdError(SD_CARD_ERROR_CMD13);
    }
  }
  return true;
}
//------------------------------------------------------------------------------
bool SdioCard::syncDevice() {
  if (!waitTransferComplete()) {
    return false;
  }
  if (m_curState != IDLE_STATE) {
    return stopTransmission(true);
  }
  return true;
}
//------------------------------------------------------------------------------
uint8_t SdioCard::type() const {
  return  m_version2 ? m_highCapacity ?
          SD_CARD_TYPE_SDHC : SD_CARD_TYPE_SD2 : SD_CARD_TYPE_SD1;
}
//------------------------------------------------------------------------------
bool SdioCard::writeData(const uint8_t* src) {
  DBG_IRQSTAT();
  if (!waitTransferComplete()) {
    return false;
  }
  const uint32_t* p32 = reinterpret_cast<const uint32_t*>(src);
  if (!(m_psdhc->PRES_STATE & SDHC_PRSSTAT_WTA)) {
    m_psdhc->PROT_CTRL &= ~SDHC_PROCTL_SABGREQ;
    m_psdhc->PROT_CTRL |= SDHC_PROCTL_CREQ;
  }
  m_psdhc->PROT_CTRL |= SDHC_PROCTL_SABGREQ;
  if (waitTimeout(&SdioCard::isBusyFifoWrite)) {
    return sdError(SD_CARD_ERROR_WRITE_FIFO);
  }
  for (uint32_t iw = 0 ; iw < 512/(4*FIFO_WML); iw++) {
    while (0 == (m_psdhc->PRES_STATE & SDHC_PRSSTAT_BWEN)) {
    }
    for (uint32_t i = 0; i < FIFO_WML; i++) {
      m_psdhc->DATA_BUFF_ACC_PORT = p32[i];
    }
    p32 += FIFO_WML;
  }
  m_transferActive = true;
  return true;
}
//------------------------------------------------------------------------------
bool SdioCard::writeSector(uint32_t sector, const uint8_t* src) {
  if (m_sdioConfig.useDma()) {
    uint8_t* ptr;
    uint8_t aligned[512];
    if (3 & (uint32_t)src) {
      ptr = aligned;
      memcpy(aligned, src, 512);
    } else {
      ptr = const_cast<uint8_t*>(src);
    }
    if (!rdWrSectors(CMD24_DMA_XFERTYP, sector, ptr, 1)) {
      return sdError(SD_CARD_ERROR_CMD24);
    }
  } else {
    if (!waitTransferComplete()) {
      return false;
    }
    if (m_curState != WRITE_STATE || m_curSector != sector) {
      if (!syncDevice()) {
        return false;
      }
      if (!writeStart(sector )) {
        return false;
      }
      m_curSector = sector;
      m_curState = WRITE_STATE;
    }
    if (!writeData(src)) {
      return false;
    }
    m_curSector++;
  }
  return true;
}
//------------------------------------------------------------------------------
bool SdioCard::writeSectors(uint32_t sector, const uint8_t* src, size_t n) {
  if (m_sdioConfig.useDma()) {
    uint8_t* ptr = const_cast<uint8_t*>(src);
    if (3 & (uint32_t)ptr) {
      for (size_t i = 0; i < n; i++, sector++, ptr += 512) {
        if (!writeSector(sector, ptr)) {
          return false;  // writeSector will set errorCode.
        }
      }
      return true;
    }
    if (!rdWrSectors(CMD25_DMA_XFERTYP, sector, ptr, n)) {
      return sdError(SD_CARD_ERROR_CMD25);
    }
  } else {
    for (size_t i = 0; i < n; i++) {
      if (!writeSector(sector + i, src + i*512UL)) {
        return false;
      }
    }
  }
  return true;
}
//------------------------------------------------------------------------------
bool SdioCard::writeStart(uint32_t sector) {
  if (yieldTimeout(&SdioCard::isBusyCMD13)) {
    return sdError(SD_CARD_ERROR_CMD13);
  }
  m_psdhc->PROT_CTRL &= ~SDHC_PROCTL_SABGREQ;

  // Infinite transfer.
  m_psdhc->BLK_ATT = SDHC_BLKATTR_BLKSIZE(512);
  if (!cardCommand(CMD25_PGM_XFERTYP, m_highCapacity ? sector : 512*sector)) {
    return sdError(SD_CARD_ERROR_CMD25);
  }
  return true;
}
//------------------------------------------------------------------------------
bool SdioCard::writeStop() {
  return transferStop();
}
#endif  // defined(__MK64FX512__)  defined(__MK66FX1M0__) defined(__IMXRT1062__)
