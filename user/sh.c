// Shell.

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"
#include "kernel/fs.h"

// Parsed command representation
#define EXEC  1   // 普通命令,如 ls
#define REDIR 2   // 重定向命令,如 cat < file
#define PIPE  3   // 管道命令,如 ls | wc
#define LIST  4   // 顺序执行,如 cmd1; cmd2
#define BACK  5   // 后台执行,如 cmd1 &

#define MAXARGS 10

#define MAX_HISTORY_SIZE 10
#define MAX_INPUT_LENGTH 100

char history[MAX_HISTORY_SIZE][MAX_INPUT_LENGTH];
int count;
int next;
int histpos = -1;     // 浏览光标：往回数几条。0 = 最新，-1 = 没在翻历史

void save_history(char *cmd);
static int redraw(char *buf, int len, int nbuf, char *s);
static int histindex(int histpos);
static void reprint(char *buf, int len, int from, int want, int pad);

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS]; // 保留每个参数的结束位置,用于补'\0'
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

int fork1(void);  // Fork but panics on failure.
void panic(char*);
struct cmd *parsecmd(char*);
void runcmd(struct cmd*) __attribute__((noreturn));

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit(1);

  switch(cmd->type){
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit(1);
    exec(ecmd->argv[0], ecmd->argv);
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0)
      runcmd(lcmd->left);
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if(pipe(p) < 0)
      panic("pipe");
    if(fork1() == 0){
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if(fork1() == 0){
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}

// 补全行末的单词，返回补全后的输入长度
static int complete(char *buf, int len, int cap, int pos) {
  if (pos != len) return len; 

  int start = len;
  int fd, prefix, namelen, extra;
  struct dirent de;
  char name[DIRSIZ + 1];

  // 空白和 shell 操作符都是单词分隔符
  while (start > 0 && strchr(" \t\r\n\v<|>&;()", buf[start - 1]) == 0) {
    start--;
  }

  prefix = len - start;

  // 空前缀和带路径的前缀不处理
  if (prefix == 0 || prefix > DIRSIZ) return len;
  for (int i = start; i < len; i++) {
    if (buf[i] == '/') return len;
  }

  // 遍历当前目录，寻找匹配的文件名
  fd = open(".", O_RDONLY);
  if (fd < 0) return len;

  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if(de.inum == 0) continue;

    memmove(name, de.name, DIRSIZ);
    name[DIRSIZ] = 0;

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
      continue;
    }

    namelen = strlen(name);
    if (namelen < prefix || memcmp(name, buf + start, prefix) != 0) {
      continue;
    }
      

    extra = namelen - prefix;

    // 预留 '\n' 和 '\0'；容量不足就不补全。
    if (len + extra <= cap - 2) {
      memmove(buf + len, name + prefix, extra);
      write(2, name + prefix, extra);
      len += extra;
      buf[len] = 0;
    }

    break;  // 只取第一个匹配项
  }

  close(fd);
  return len;
}

int getcmd(char *buf, int nbuf, int from_file) {
  int len = 0, pos = 0;   // len = 总长，pos = 光标位置
  int result = -1;
  char c;
  struct stat st;

  if (nbuf < 2) return -1;

  memset(buf, 0, nbuf);

  // 标准输入是文件或管道时，保留原来的读取方式。
  if (fstat(0, &st) < 0 || st.type != T_DEVICE) {
    gets(buf, nbuf);
    return buf[0] == 0 ? -1 : 0;
  }

  if (consmode(1) < 0) return -1;

  if (from_file == 0) write(2, "$ ", 2);

  while (read(0, &c, 1) == 1) {
    if (c == '\n' || c == '\r') {
      buf[len++] = '\n';
      write(2, "\n", 1);
      result = 0;
      break;
    } else if (c == '\t') {
      // 补全当前输入的单词
      if (pos == len) {
        len = complete(buf, len, nbuf, pos);
        pos = len;
      } // 补全后屏幕光标在行尾，pos 要跟上
      continue;
    } else if (c == '\b' || c == 0x7f) {  // Backspace/Delete：删除当前输入的最后一个字符
      if (pos > 0) {
        memmove(buf + pos - 1, buf + pos, len - pos);
        pos--; 
        len--; 
        buf[len] = 0;
        write(2, "\b", 1);                    // 屏幕光标先退到 pos，凑齐 reprint 的前提
        reprint(buf, len, pos, pos, 1);       // 多出 1 格用空格盖掉
      }
      continue;
    } else if (c == 0x15) {  // Ctrl-U：清空当前输入
      // 先把屏幕光标送到行尾，否则光标停在中间时会把提示符一起擦掉。
      while (pos < len) {
        write(2, &buf[pos], 1);
        pos++;
      }
      while (len > 0) {
        buf[--len] = 0;
        write(2, "\b \b", 3);
      }
      pos = 0;
      continue;
    } else if (c == 0x04) {  // Ctrl-D：空行退出，否则提交当前输入
      // 光标不在行尾时先送到行尾，免得换行符插在句子中间。
      while (pos < len) {
        write(2, &buf[pos], 1);
        pos++;
      }
      if(len > 0){
        buf[len++] = '\n';
        result = 0;
      }
      write(2, "\n", 1);
      break;
    } else if (c == 0x1b) {  // ESC：可能是一个转义序列
      char seq[2];
      if (read(0, &seq[0], 1) != 1) continue;
      if (read(0, &seq[1], 1) != 1) continue;
      if (seq[0] == '[') {
        if (seq[1] == 'A') { // 向上箭头：显示历史记录中的上一条命令
          if (histpos + 1 < count) {
            while (pos < len) {
              write(2, &buf[pos], 1);
              pos++;
            }
            len = redraw(buf, len, nbuf, history[histindex(++histpos)]);
            pos = len;  // 整行被换掉了，光标回到行尾
          }
        } else if (seq[1] == 'B') { // 向下箭头：显示历史记录中的下一条命令
          if (histpos >= 0) {
            histpos--;
            while (pos < len) {
              write(2, &buf[pos], 1);
              pos++;
            }
            len = redraw(buf, len, nbuf, histpos < 0 ? 0 : history[histindex(histpos)]);
            pos = len;  // 整行被换掉了，光标回到行尾
          }
        } else if (seq[1] == 'C') {               // 向右箭头
          if (pos < len) { write(2, &buf[pos], 1); pos++; }
        } else if (seq[1] == 'D') {             // 向左箭头
          if (pos > 0) { pos--; write(2, "\b", 1); }
        }
      }
      continue;
    } 
    
    if (c >= ' ' && c <= '~' && len < nbuf - 2) {
      memmove(buf + pos + 1, buf + pos, len - pos);
      buf[pos] = c;
      pos++; 
      len++; 
      buf[len] = 0;
      reprint(buf, len, pos - 1, pos, 0);     // 从新字符那格画到行尾
    }
  }

  buf[len] = 0;
  consmode(0);
  return result;
}

// int
// getcmd(char *buf, int nbuf, int from_file)
// {
//   if (from_file == 0) write(2, "$ ", 2);
//   memset(buf, 0, nbuf);
//   gets(buf, nbuf);
//   if(buf[0] == 0) // EOF
//     return -1;
//   return 0;
// }

int
main(void)
{
  static char buf[100];
  int fd;

  // Ensure that three file descriptors are open.
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // Read and run input commands.
  struct stat st;
  int from_file;

  if(fstat(0, &st) < 0) exit(1);
  from_file = (st.type != T_DEVICE);

  while(getcmd(buf, sizeof(buf), from_file) >= 0){
    save_history(buf);
    char *cmd = buf;
    while (*cmd == ' ' || *cmd == '\t')
      cmd++;
    if (*cmd == '\n') // is a blank command
      continue;

    if (strcmp(cmd, "wait\n") == 0 || strcmp(cmd, "wait") == 0) {
      while (wait(0) > 0);
    } else if(cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' '){
      // Chdir must be called by the parent, not the child.
      cmd[strlen(cmd)-1] = 0;  // chop \n
      if(chdir(cmd+3) < 0)
        fprintf(2, "cannot cd %s\n", cmd+3);
    } else {
      int end = strlen(cmd);
      int background = 0;
      int pid, done;

      while (end > 0 && strchr(" \t\r\n\v", cmd[end - 1])) end--; // 去掉末尾空白
      if (end > 0 && cmd[end - 1] == '&') {
        // 行尾 & 表示后台执行，由父 shell 处理
        background = 1;
        cmd[end - 1] = 0; 
      }

      pid = fork1();
      if(pid == 0) runcmd(parsecmd(cmd));

      if (!background) {
        // 必须等到本次前台任务结束。
        while ((done = wait(0)) > 0) {
          if (done == pid)
            break;
        }
      }
    }
  }
  exit(0);
}

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

//PAGEBREAK!
// Constructors

struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}
//PAGEBREAK!
// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if(*s == '>'){
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;

  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *nulterminate(struct cmd*);

struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE|O_TRUNC, 1);
      break;
    case '+':  // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd*
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if(!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if(peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|)&;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
struct cmd*
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    return 0;

  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}

void save_history(char *cmd) {
  int len = strlen(cmd);
  if (len > 0 && cmd[len - 1] == '\n') len--;
  histpos = -1;                       // 光标归位
  if (len == 0) return;
  memmove(history[next], cmd, len);
  history[next][len] = '\0';
  next = (next + 1) % MAX_HISTORY_SIZE;
  if (count < MAX_HISTORY_SIZE) count++;
}

// 把当前输入行换成 s，并返回新行的长度
static int redraw(char *buf, int len, int nbuf, char *s) {
  int n = s ? strlen(s) : 0;

  if (n > nbuf - 2) n = nbuf - 2;      // 给 '\n' 和 '\0' 留位置
  while (len > 0) {                    // 擦掉当前这一行
    buf[--len] = 0;
    write(2, "\b \b", 3);
  }
  if (n > 0) {
    memmove(buf, s, n);
    buf[n] = 0;
    write(2, buf, n);
  }
  return n;
}

// 根据 histpos 计算 history 数组的索引
static int histindex(int histpos) {
  int p = next - 1 - histpos;
  if (p < 0) p += MAX_HISTORY_SIZE;
  return p;
}

// 从 from 开始把 buf[from..len) 重画出来，用 pad 个空格盖掉屏幕上残留的旧字符，最后把光标停在 want
static void reprint(char *buf, int len, int from, int want, int pad) {
  int i, back = len + pad - want;

  write(2, buf + from, len - from);
  for (i = 0; i < pad; i++) write(2, " ", 1);
  for (i = 0; i < back; i++) write(2, "\b", 1);
}
