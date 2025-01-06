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
#ifndef SdioCard_h
#define SdioCard_h
#include "../common/SysCall.h"
#include "SdCardInterface.h"

#define FIFO_SDIO 0
#define DMA_SDIO 1

#define USE_SDIO2 2

#if 0 //defined(__IMXRT1062__)
// Temporary experiment 
// Define as class -> move to imxrt.h
typedef struct {
        volatile uint32_t DS_ADDR;                  //offset000)
        volatile uint32_t BLK_ATT;                  //offset004)
        volatile uint32_t CMD_ARG;                  //offset008)
        volatile uint32_t CMD_XFR_TYP;              //offset00C)
        volatile uint32_t CMD_RSP0;                 //offset010)
        volatile uint32_t CMD_RSP1;                 //offset014)
        volatile uint32_t CMD_RSP2;                 //offset018)
        volatile uint32_t CMD_RSP3;                 //offset01C)
        volatile uint32_t DATA_BUFF_ACC_PORT;       //offset020)
        volatile uint32_t PRES_STATE;               //offset024)
        volatile uint32_t PROT_CTRL;                //offset028)
        volatile uint32_t SYS_CTRL;                 //offset02C)
        volatile uint32_t INT_STATUS;               //offset030)
        volatile uint32_t INT_STATUS_EN;            //offset034)
        volatile uint32_t INT_SIGNAL_EN;            //offset038)
        volatile uint32_t AUTOCMD12_ERR_STATUS;     //offset03C)
        volatile uint32_t HOST_CTRL_CAP;            //offset040)
        volatile uint32_t WTMK_LVL;                 //offset044)
        volatile uint32_t MIX_CTRL;                 //offset048)
        uint32_t unused1;
        volatile uint32_t FORCE_EVENT;              //offset050)
        volatile uint32_t ADMA_ERR_STATUS;          //offset054)
        volatile uint32_t ADMA_SYS_ADDR;            //offset058)
        uint32_t unused2;
        volatile uint32_t DLL_CTRL;                 //offset060)
        volatile uint32_t DLL_STATUS;               //offset064)
        volatile uint32_t CLK_TUNE_CTRL_STATUS;     //offset068)
        uint32_t unused3[21];                       //6c 70 80 90 A0 B0
        volatile uint32_t VEND_SPEC;                //offset0C0)
        volatile uint32_t MMC_BOOT;                 //offset0C4)
        volatile uint32_t VEND_SPEC2;               //offset0C8)
        volatile uint32_t TUNING_CTRL;              //offset0CC)
} IMXRT_USDHC_t;

#endif

/**
 * \class SdioConfig
 * \brief SDIO card configuration.
 */
class SdioConfig {
 public:
  SdioConfig() {}
  /**
   * SdioConfig constructor.
   * \param[in] opt SDIO options.
   */
  explicit SdioConfig(uint8_t opt) : m_options(opt) {}
  /** \return SDIO card options. */
  uint8_t options() {return m_options;}
  /** \return true if DMA_SDIO. */
  bool useDma() {return m_options & DMA_SDIO;}
 private:
  uint8_t m_options = FIFO_SDIO;
};
//------------------------------------------------------------------------------
/**
 * \class SdioCard
 * \brief Raw SDIO access to SD and SDHC flash memory cards.
 */
class SdioCard : public SdCardInterface {
 public:
  /** Initialize the SD card.
   * \param[in] sdioConfig SDIO card configuration.
   * \return true for success or false for failure.
   */
  bool begin(SdioConfig sdioConfig);
  /** Disable an SDIO card.
   * not implemented.
   */
  void end() {}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
    uint32_t __attribute__((error("use sectorCount()"))) cardSize();
#endif  // DOXYGEN_SHOULD_SKIP_THIS
  /** Erase a range of sectors.
   *
   * \param[in] firstSector The address of the first sector in the range.
   * \param[in] lastSector The address of the last sector in the range.
   *
   * \note This function requests the SD card to do a flash erase for a
   * range of sectors.  The data on the card after an erase operation is
   * either 0 or 1, depends on the card vendor.  The card must support
   * single sector erase.
   *
   * \return true for success or false for failure.
   */
  bool erase(uint32_t firstSector, uint32_t lastSector);
  /**
   * \return code for the last error. See SdCardInfo.h for a list of error codes.
   */
  uint8_t errorCode() const;
  /** \return error data for last error. */
  uint32_t errorData() const;
  /** \return error line for last error. Tmp function for debug. */
  uint32_t errorLine() const;
  /**
   * Check for busy with CMD13.
   *
   * \return true if busy else false.
   */
  bool isBusy();
  /** \return the SD clock frequency in kHz. */
  uint32_t kHzSdClk();
  /**
   * Read a 512 byte sector from an SD card.
   *
   * \param[in] sector Logical sector to be read.
   * \param[out] dst Pointer to the location that will receive the data.
   * \return true for success or false for failure.
   */
  bool readSector(uint32_t sector, uint8_t* dst);
  /**
   * Read multiple 512 byte sectors from an SD card.
   *
   * \param[in] sector Logical sector to be read.
   * \param[in] ns Number of sectors to be read.
   * \param[out] dst Pointer to the location that will receive the data.
   * \return true for success or false for failure.
   */
  bool readSectors(uint32_t sector, uint8_t* dst, size_t ns);
  /**
   * Read a card's CID register. The CID contains card identification
   * information such as Manufacturer ID, Product name, Product serial
   * number and Manufacturing date.
   *
   * \param[out] cid pointer to area for returned data.
   *
   * \return true for success or false for failure.
   */
  bool readCID(cid_t* cid);
  /**
   * Read a card's CSD register. The CSD contains Card-Specific Data that
   * provides information regarding access to the card's contents.
   *
   * \param[out] csd pointer to area for returned data.
   *
   * \return true for success or false for failure.
   */
  bool readCSD(csd_t* csd);
  /** Read one data sector in a multiple sector read sequence
   *
   * \param[out] dst Pointer to the location for the data to be read.
   *
   * \return true for success or false for failure.
   */
  bool readData(uint8_t* dst);
  /** Read OCR register.
   *
   * \param[out] ocr Value of OCR register.
   * \return true for success or false for failure.
   */
  bool readOCR(uint32_t* ocr);
  /** Start a read multiple sectors sequence.
   *
   * \param[in] sector Address of first sector in sequence.
   *
   * \note This function is used with readData() and readStop() for optimized
   * multiple sector reads.  SPI chipSelect must be low for the entire sequence.
   *
   * \return true for success or false for failure.
   */
  bool readStart(uint32_t sector);
  /** Start a read multiple sectors sequence.
   *
   * \param[in] sector Address of first sector in sequence.
   * \param[in] count Maximum sector count.
   * \note This function is used with readData() and readStop() for optimized
   * multiple sector reads.  SPI chipSelect must be low for the entire sequence.
   *
   * \return true for success or false for failure.
   */
  bool readStart(uint32_t sector, uint32_t count);
  /** End a read multiple sectors sequence.
   *
   * \return true for success or false for failure.
   */
  bool readStop();
  /** \return SDIO card status. */
  uint32_t status();
    /**
   * Determine the size of an SD flash memory card.
   *
   * \return The number of 512 byte data sectors in the card
   *         or zero if an error occurs.
   */
  uint32_t sectorCount();
  /**
   *  Send CMD12 to stop read or write.
   *
   * \param[in] blocking If true, wait for command complete.
   *
   * \return true for success or false for failure.
   */
  bool stopTransmission(bool blocking);
  /** \return success if sync successful. Not for user apps. */
  bool syncDevice();
  /** Return the card type: SD V1, SD V2 or SDHC
   * \return 0 - SD V1, 1 - SD V2, or 3 - SDHC.
   */
  uint8_t type() const;
  /**
   * Writes a 512 byte sector to an SD card.
   *
   * \param[in] sector Logical sector to be written.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  bool writeSector(uint32_t sector, const uint8_t* src);
  /**
   * Write multiple 512 byte sectors to an SD card.
   *
   * \param[in] sector Logical sector to be written.
   * \param[in] ns Number of sectors to be written.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  bool writeSectors(uint32_t sector, const uint8_t* src, size_t ns);
  /** Write one data sector in a multiple sector write sequence.
   * \param[in] src Pointer to the location of the data to be written.
   * \return true for success or false for failure.
   */
  bool writeData(const uint8_t* src);
  /** Start a write multiple sectors sequence.
   *
   * \param[in] sector Address of first sector in sequence.
   *
   * \note This function is used with writeData() and writeStop()
   * for optimized multiple sector writes.
   *
   * \return true for success or false for failure.
   */
  bool writeStart(uint32_t sector);
  /** Start a write multiple sectors sequence.
   *
   * \param[in] sector Address of first sector in sequence.
   * \param[in] count Maximum sector count.
   * \note This function is used with writeData() and writeStop()
   * for optimized multiple sector writes.
   *
   * \return true for success or false for failure.
   */
  bool writeStart(uint32_t sector, uint32_t count);

  /** End a write multiple sectors sequence.
   *
   * \return true for success or false for failure.
   */
  bool writeStop();

 private:
  static const uint8_t IDLE_STATE = 0;
  static const uint8_t READ_STATE = 1;
  static const uint8_t WRITE_STATE = 2;
  uint32_t m_curSector;
  SdioConfig m_sdioConfig;
  uint8_t m_curState = IDLE_STATE;
#if defined(__IMXRT1062__)
  // move helper functions into class.
  typedef bool (SdioCard::*pcheckfcn)();
  bool cardCommand(uint32_t xfertyp, uint32_t arg);
  void enableGPIO(bool enable, bool fUseSDIO2);
  void enableDmaIrs();
  void initSDHC();
  bool isBusyCMD13();
  bool isBusyCommandComplete();
  bool isBusyCommandInhibit();
  bool readReg16(uint32_t xfertyp, void* data);
  void setSdclk(uint32_t kHzMax);
  bool yieldTimeout(pcheckfcn fcn);
  bool waitDmaStatus();
  bool waitTimeout(pcheckfcn fcn);
  inline bool setSdErrorCode(uint8_t code, uint32_t line);

/////////////////////////////////////////////
 static void   sdIrs();
 static void   sdIrs2(); // one for second SDIO
 static SdioCard *s_pSdioCards[2];
 void   gpioMux(uint8_t mode, bool fUseSDIO2);
 void   initClock(bool fUseSDIO2);
 uint32_t   baseClock();
 bool   cardAcmd(uint32_t rca, uint32_t xfertyp, uint32_t arg);
 bool   cardCMD6(uint32_t arg, uint8_t* status);
 uint32_t   statusCMD13();
 bool   isBusyDat();
 bool   isBusyDMA();
 bool   isBusyFifoRead();
 bool   isBusyFifoWrite();
 bool   isBusyTransferComplete();
 bool   rdWrSectors(uint32_t xferty, uint32_t sector, uint8_t* buf, size_t n);

 bool   transferStop();
 bool   waitTransferComplete();
 void   printRegs(uint32_t line);
/////////////////////////////////////////////////////
  // lets move glbabal (static) variables into class instance.
  IMXRT_USDHC_t *m_psdhc;
  pcheckfcn m_busyFcn = nullptr;
  bool m_initDone = false;
  bool m_version2;
  bool m_highCapacity;
  bool m_transferActive = false;
  uint8_t m_errorCode = SD_CARD_ERROR_INIT_NOT_CALLED;
  uint32_t m_errorLine = 0;
  uint32_t m_rca;
  volatile bool m_dmaBusy = false;
  volatile uint32_t m_irqstat;
  uint32_t m_sdClkKhz = 0;
  uint32_t m_ocr;
  cid_t m_cid;
  csd_t m_csd;

#endif
};

#endif  // SdioCard_h
