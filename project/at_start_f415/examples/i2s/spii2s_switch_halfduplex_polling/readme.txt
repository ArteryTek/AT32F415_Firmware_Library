/**
  **************************************************************************
  * @file     readme.txt
  * @brief    readme
  **************************************************************************
  */

  this demo is based on the at-start board, in this demo, shows in halfduplex
  mode how to use polling transfer data. use the mode switch to realize spi and
  i2s communication.
  the pins connection as follow:
  - pb0 as mck out
  - i2s2 slaver        i2s1 master
  - pb12     <--->     pa15(ws)
  - pb13     <--->     pb3(ck)
  - pb15     <--->     pb5(sd)

  for more detailed information. please refer to the application note document AN0102.

