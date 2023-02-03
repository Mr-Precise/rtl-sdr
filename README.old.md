  rtl-sdr
  turns your Realtek RTL2832 based DVB dongle into a SDR receiver
  ======================================================================
  Experimental R820T and R820T2 rtl-sdr driver that tunes down to 13 mhz or lower.  
  (From my experiments up to 3.4 Mhz on R820T2)

  2017-2018: Transferring new features from new versions and fixes...  
  2021: Project is now using only cmake build system.  
  Integrated RTL_NFC code from https://github.com/Iskuri/RTLSDR-NFC  
  thanks for Iskuri.

  
  ### Setup for SDR only use - without DVB compatibility:

- a special USB vendor/product id got reserved at http://pid.codes/ : 0x1209/0x2832
- for such devices the linux kernel's DVB modules are not loaded automatically,
 thus can be used without blacklisting dvb_usb_rtl28xxu below /etc/modprobe.d/
- this allows to use a second RTL dongle for use with DVB in parallel
- the IDs can be programmed with 'rtl_eeprom -n' or 'rtl_eeprom -g realtek_sdr'  

## (old) Notes on the mutability tree (2014/09/29)
  =========================================
  For more information see:
  https://osmocom.org/projects/rtl-sdr/wiki 
  Since there seems to be some wider interest in this, some quick notes on
  my changes here.

  TL;DR:

  0Hz  
  .  
  .  "no mod direct sampling"  
  .  no mixing at all, tuner is mostly disabled, just ram as much signal  
  .  as you can at it and maybe some will leak through.  
  .  YMMV a lot  
  .  
  ~13MHz  (PLL lower limit - 14MHz)  
  .  
  .  normal tuning, large IF.  
  .  high-side mixing  
  .  you will see nasty aliasing / attentuation / harmonics  
  .  near the lower edge of the range  
  .  you will see lots of noise from the dongle itself near 14.4MHz  
  .  
  ~21MHz  (PLL lower limit - 6MHz)   <- upstream tuner lower limit  
  .  
  .  normal tuning, regular IF  
  .  high-side mixing  
  .  this should behave much like upstream  
  .  
  ~1844Mhz (PLL upper limit - 6MHz)  <- upstream tuner upper limit  
  .  
  .  normal tuning, small IF (getting squashed against the PLL upper bound)  
  .  high-side mixing  
  .  
  ~1848MHz (PLL upper limit - 2MHz)  
  .  
  .  tuning with low-side mixing (PLL frequency below tuned frequency)  
  .  you will see nasty aliasing / attentuation / harmonics
  .  near the upper edge of the range  
  .  
  ~1864MHz (PLL upper limit + 14MHz) <- that's all, folks  
  
  (PLL limits vary by dongle - some go as high as 1885MHz)

  This tree is a collection of random tuner hacks which are really exploratory
  more than anything. They may or may not work for you.

  First some background for those unfamiliar with the internals of the dongle:

* The R820T tuner has a tunable PLL that can generate frequencies between
  around 27MHz .. 1850MHz. The exact range varies from dongle to dongle.

* The tuner mixes the incoming RF signal with this PLL output. This shifts
  the RF signal down to a frequency that is the difference between the PLL
  frequency and the RF signal's frequency.

* The tuner sends this intermediate frequency (IF) signal to the RTL2838U

* The RTL2838U digitizes the IF signal and does a digital downconversion
  step to generate I/Q samples that are centered around "zero". The
  downconverter can theoretically handle IF signals up to 14.4MHz.
  
  The main change is to feed information about the actually-tuned frequency back
  from the tuner (R820T) code to the core (RTL2832U) code, allowing the core
  code to adjust to the actual IF in use, rather than requiring a fixed IF. The
  core code has also been taught how to handle low-side mixing, where the PLL
  frequency is lower than the requested frequency (e.g. the spectrum inversion
  changes then)
  
  When tuning, the R820T tuner will try to tune to the requested frequency +
  6MHz, producing a 6MHz IF. If the PLL cannot handle that frequency, the tuner
  will try a few things to produce something useful.
  
  At the top of the tuning range:

* It will tune as high as it can go normally, then stick there producing a
  smaller effective IF as the requested frequency gets higher and higher.

* Once the IF starts getting too small (below about 1.5MHz things start to
  go bad), it will switch to low-side mixing and try to put the PLL frequency
  *below* the target frequency. This gives you another 14-15 MHz at the top
  of the range.
  
  I've had reports that some dongles can tune to within a whisker of 2000MHz
  with this.
  
  At the bottom of the range:

* It will tune as low as it can go normally, then stick there producing a
  larger effective IF as the requested frequency gets lower and lower.

* Once the required IF exceeds 14.4MHz, it will switch to a variant of tejeez'
  et al "no mod direct sampling" mode. This essentially disables the PLL in
  the tuner entirely, and relies on some RF signal leaking through the tuner
  unchanged. The tuner claims to be tuned to 0Hz in this mode, and the core
  does all the real tuning. The dongle is almost deaf in this mode, you will
  have to turn the RF gain WAY UP. You do not need to mess with direct sampling
  settings to enable this, it's integrated with normal tuning.
  
  The success of the "no mod direct sampling" mode seems to vary a lot from dongle
  to dongle. Mine is almost completely deaf so I can't test this much. Others have
  had success in tuning to shortwave stations as low as 3.5MHz.
  
  The actual PLL limits vary from dongle to dongle, and from day to day (they're
  probably temperature related). The tuner has three sets of tuning limits:

* a hardcoded "do not exceed this" set of limits - see PLL_INITIAL_LOW /
  PLL_INITIAL_HIGH in tuner_r82xx.c. These are in place because, especially at
  the low end of the range, the PLL can get into a state where you ask for (for
  example) 25MHz, the PLL claims to be locked OK at that frequency, but in
  reality it's actually producing 27MHz, which screws up the core's calculations
  of the IF offset needed.

* a hardcoded "known to be OK" set of limits - see PLL_SAFE_LOW / PLL_SAFE_HIGH
  in tuner_r82xx.c. This is a range where the PLL should always work; if the PLL
  doesn't work in this range it is treated as a tuning error and tuning fails.

* a runtime "seems to be OK at the moment" set of limits. This varies from run
  to run and initially starts at the "do not exceed" limits. Whenever a failure
  to get PLL lock is seen, the runtime limits are shrunk accordingly and we try
  again. This allows the tuner to adapt to the particular dongle in use.
  
  Remember that at the extreme edges of these ranges you will be fighting an uphill
  battle against the dongle design itself, as you will be far out of the intended
  range of the filters etc and nothing works particularly well.
  
  You are likely to see small frequency shifts (maybe 1-2ppm) from what you get in
  upstream. This is because the tuner+2832 combination can be tuned a little more
  accurately than upstream does, so some errors that you will have previously been
  correcting for will disappear.

  -- Oliver (oliver.jowett@gmail.com / oliver@mutability.co.uk)
