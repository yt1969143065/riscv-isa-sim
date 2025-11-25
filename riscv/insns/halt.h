printf("\33[1;34mspike: %s\33[0m at pc = 0x%016lx\33[0m\n",
       (STATE.XPR[10] == 0 ? "\33[1;32mHIT GOOD TRAP" : "\33[1;31mHIT BAD TRAP"),
       STATE.pc);
exit(0);
