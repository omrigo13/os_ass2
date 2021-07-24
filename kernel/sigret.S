
.globl signal_return_start
.globl signal_return_end

signal_return_start:
  li a7, 24
  ecall
  ret              
signal_return_end: