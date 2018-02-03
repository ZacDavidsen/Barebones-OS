/* Written by (Team 02) Chris Nurrenberg, Zac Davidsen, Trey Lewis 1/31/18*/

#include "./syscall.h"
#include "./keycode.h"

int strnCmp(char *str1, char *str2, int length);
int strnCpy(char *src, char *dest, int length);
int strLen(char *str);
void parseArguments(char *args, int *argc, char **argv);
void listDirectory();
void creatFile(char *name);
void clearScreen();
void drawStuff();

int main() {
  char cmdBuf[80];
  int i;
  char buffer[13312];
  int argCount;
  char *argArray[5];
  int readCount;
  unsigned char currCol;
  int pid;
  currCol = 0;

  enableInterrupts();
  while (1) {
    for (i = 0; i < 80; i++) {
      cmdBuf[i] = 0;
    }
    for (i = 0; i < 13312; i++) {
      buffer[i] = 0;
    }
  

    interrupt(PRINTSTRING, "SHELL: ", 0, 0);
    interrupt(READSTRING, cmdBuf, 0, 0);

    parseArguments(cmdBuf, &argCount, argArray);

    if (strnCmp(argArray[0], "type", 5) == 0 && argCount >= 2) {
      readFile(argArray[1], buffer, &readCount);
      if (readCount > 0) {
        printString(buffer);
        printString("\n\r");
      } else {
        printString("FILE NOT FOUND\n\r");
      }
    } else if (strnCmp(argArray[0], "execute", 8) == 0 && argCount >= 2) {
      executeProgram(argArray[1], 0);
    } else if (strnCmp(argArray[0], "delete", 7) == 0) {
      deleteFile(argArray[1]);
    } else if (strnCmp(argArray[0], "copy", 5) == 0 && argCount == 3) {
      readFile(argArray[1], buffer, &readCount);
      if (readCount > 0) {
        writeFile(argArray[2], buffer, readCount);
      } else {
        printString("FILE NOT FOUND\n\r");
      }
    } else if (strnCmp(argArray[0], "dir", 4) == 0 && argCount == 1) {
      listDirectory();
    } else if (strnCmp(argArray[0], "create", 7) == 0 && argCount == 2) {
      creatFile(argArray[1]);
    } else if (strnCmp(argArray[0], "clear", 6) == 0) {
      clearScreen();
    } else if (strnCmp(argArray[0], "bgcol", 6) == 0) {
      /* keycode hack, cycles background color */
      interrupt(0x10, 0x0b00, ++currCol, 0, 0);
    } else if (strnCmp(argArray[0], "pg2", 4) == 0) {
      interrupt(0x10, 0x0501, 0x80);
    } else if (strnCmp(argArray[0], "pg1", 4) == 0) {
      interrupt(0x10, 0x0500, 0x80);
    } else if (strnCmp(argArray[0], "draw", 5) == 0) {
      drawStuff();
    } else if (strnCmp(argArray[0], "kill", 5) == 0) {
      killProcess(argArray[1][0] - 0x30);
    } else if (strnCmp(argArray[0], "execforeground", 15) == 0) {
      executeProgram(argArray[1], &pid);
      blockProcess(pid);
    } else {
      printString("Invalid Command\n\r");
    }
  }
}

int strnCpy(char *src, char *dest, int length) {
  int i;
  for (i = 0; i < length; i++) {
    if (src[i] == 0) {
      break;
    }
    dest[i] = src[i];
  }
  return i;
}

int strnCmp(char *str1, char *str2, int length) {
  int i, ret;

  for (i = 0; i < length; i++) {
    ret = str1[i] - str2[i];
    if (ret != 0)
      return ret;

    if (str1[i] == 0)
      return 0;
  }
  return 0;
}

int strLen(char *str) {
  int i;
  i = 0;
  while (str[i] != 0) {
    i++;
  }
  return i;
}

void parseArguments(char *args, int *argc, char **argv) {
  int i, inArg;
  *argc = 0;
  inArg = 0;


  i = 0;
  while (args[i] != '\0') {
    if (args[i] != ' ') {
      if (!inArg) {
        (argv)[*argc] = args + i;
        (*argc)++;
      }
      inArg = 1;
    } else if (args[i] == ' ') {
      inArg = 0;
      args[i] = 0x0;
    }
    i++;
  }
}

int div(int a, int b) {
  int i;
  i = 0;
  while (a >= b) {
    a = a - b;
    i++;
  }
  return i;
}

void listDirectory() {
  char dir[512], line[20];
  int i, j, count, charsCopied;

  strnCpy("FILENAME     SIZE\n\r\0", line, 20);

  printString(line);

  readSector(dir, 2);
  for (i = 0; i < 16; i++) {
    count = 0;
    if (dir[32 * i] == 0) {
      continue;
    }
    charsCopied = strnCpy(dir + 32 * i, line, 6);
    for (; charsCopied < 13; charsCopied++) {
      line[charsCopied] = ' ';
    }
    j = 6;
    while (dir[32 * i + j] != 0) {
      count++;
      j++;
    }
    line[charsCopied++] = 0x30 + div(count, 10);
    line[charsCopied++] = 0x30 + (count - 10 * div(count, 10));
    line[charsCopied++] = '\n';
    line[charsCopied++] = '\r';
    line[charsCopied] = '\0';
    printString(line);
  }
  printString("\n\r");
}

void creatFile(char *name) {
  char map[512], dir[512], line[80], writeBuffer[13312];
  int i, writeIndex, count;

  readSector(map, 1);
  readSector(dir, 2);

  for (i = 0; i < 13312; i++) {
    writeBuffer[i] = 0;
  }
  /* find available sectors in map*/

  writeIndex = 0;
  while (1) {
    readString(line);
    count = strLen(line);

    if (count == 0) {
      /* write buffer to next sector*/
      writeFile(name, writeBuffer, div(writeIndex, 512) + 1);
      return;
    }
    if (count + writeIndex > 13312) {
      strnCpy(line, writeBuffer + writeIndex, 13312 - writeIndex);
      writeFile(line, writeBuffer, 26);
      printString("File too long, truncating previous line.\n\r\0");
    } else {
      strnCpy(line, writeBuffer + writeIndex, count);
      writeIndex += count;
    }
  }
}

void clearScreen() {
  int i;
  for (i = 0; i < 25; i++) {
    printString("\n");
  }
  interrupt(0x10, 0x0200, 0, 0, 0);
}

#define WIDTH 640
#define HEIGHT 200

void drawStuff() {
  int keycode, x, y, size, i, j;
  char color;
  x = 0; y = 0; size = 4;
  color = 1;
  //change video mode to graphics
  interrupt(0x10, 0x000E, 0, 0, 0);
  while (1) {
    getKeypress(&keycode);
    
    switch (keycode) {
    case KEY_ESCAPE:
      goto end;
    case KEY_I:
    case KEY_ARROW_UP: //up arrow
      y -= size;
      if (y < 0) y = 0;
      break;
    case KEY_K:
    case KEY_ARROW_DOWN: //down arrow
      y += size;
      if (y > HEIGHT - size) y = HEIGHT - size;
      break;
    case KEY_J:
    case KEY_ARROW_LEFT: //left arrow
      x -= size * 2;
      if (x < 0) x = 0;
      break;
    case KEY_L:
    case KEY_ARROW_RIGHT: //right arrow
      x += size * 2;
      if (x > WIDTH - size * 2) x = WIDTH - size * 2;
      break;
    case KEY_1: case KEY_KP_1:
    case KEY_2: case KEY_KP_2:
    case KEY_3: case KEY_KP_3:
    case KEY_4: case KEY_KP_4:
    case KEY_5: case KEY_KP_5:
    case KEY_6: case KEY_KP_6:
    case KEY_7: case KEY_KP_7:
    case KEY_8: case KEY_KP_8:
    case KEY_9: case KEY_KP_9:
    case KEY_0: case KEY_KP_0:
      color = KEYCODE_TO_ASCII(keycode) - 0x30;
      break;
    case KEY_A:
    case KEY_B:
    case KEY_C:
    case KEY_D:
    case KEY_E:
    case KEY_F:
      color = keycode - 0x61 + 10;
      break;
    case KEY_MINUS: case KEY_KP_MINUS:
      size >>= 1;
      if (size < 1) size = 1;
      break;
    case KEY_EQUAL: case KEY_KP_PLUS:
      size <<= 1;
      if (size > 8) size = 8;
      break;
    case KEY_SPACE:
      for (i = 0; i < size * 2 && x + i < WIDTH; i++) {
        for (j = 0; j < size && y + j < HEIGHT; j++) {
          interrupt(0x10, 0x0c00 + color, 0, x + i, y + j);
        }
      }
      break;
    default: break;
    }
  }
end:
  //change video mode to text
  interrupt(0x10, 0x0003, 0, 0, 0);
}