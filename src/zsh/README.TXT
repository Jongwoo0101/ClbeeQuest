이 코드가 제대로 동작하려면 'data/campus' 디렉토리의 실제 데이터 파일들이 필요합니다.

7단계에 걸쳐 프로젝트를 다듬고 코드가 실제로 돌아가게끔 만들어준 동료에게 
깊은 감사를 전합니다. 프로젝트라는 것은 몇 달만 방치해도 금세 썩어버리기 마련이고, 
그걸 다시 끄집어내어 작동하게 만드는 데는 꽤 많은 노력이 필요하니까요.

나쁜 소식: 이 코드는 POSIX 환경(Linux, macOS, WSL)에서만 컴파일되고 실행됩니다. 
fork(), execvp(), waitpid() 같은 시스템 콜에 강하게 의존하고 있기 때문에 
Windows 네이티브 환경에서는 돌아가지 않을 것입니다. (물론 Cygwin이나 WSL을 
쓴다면 아무 문제 없습니다.)

그럼에도 불구하고 코드는 꽤 이식성이 높으며, C11 표준을 준수하기 때문에 
어느 정도의 유닉스 호환 환경이라면 쉽게 포팅할 수 있을 것입니다.

이 코드를 처음부터 차근차근 짜 올라가면서, 지금 돌아보면 정말이지 
우스꽝스럽게 느껴지는 부분들이 꽤 있습니다. (캠퍼스 데이터를 처리한답시고 
'|' 기호로 구분된 평문 텍스트 파일을 루프마다 처음부터 끝까지 선형 탐색하는 
방식이 제일 먼저 떠오르네요.) 하지만 전반적으로 이 코드는 쉘의 기본 동작 
원리를 실험하고 발전시키기 위한 아주 유용한 베이스캠프가 될 것입니다.

파서에서 백그라운드 프로세스 관리로 이어지는 파이프라인의 기본 개념 자체는 
정확했습니다. 하지만 다시 짤 기회가 주어진다면 원래 코드보다 훨씬 더 극적으로 
개선할 수 있을 겁니다. 백그라운드 작업 상태를 추적하기 위해 REPL 루프가 
돌 때마다 waitpid(WNOHANG)를 무식하게 호출하는 방식보다는, SIGCHLD 시그널 
핸들러를 제대로 구현하여 비동기적으로 처리하는 것이 '올바른 길(The Right Thing)'
이었을 것입니다. 

파이프('|')와 리다이렉션('>', '<')을 이번 단계에서 명시적으로 막아둔 것 
역시 제가 되돌아보며 느끼는 큰 아쉬움 중 하나입니다. 토큰을 분리하는 김에 
AST(추상 구문 트리)를 도입했다면 파이프라인 구현이 훨씬 쉬웠을 텐데 말이죠.

만약 이 코드를 가지고 놀 분들을 위한 몇 가지 프로젝트 아이디어를 제안합니다:

 - 텍스트 파일(.txt)을 뒤지는 방식 대신, 실제 학교 서버나 공공 데이터 REST API를 
   연동하여 데이터를 파싱하도록 변경해 보세요.
 - 진짜 파이프('|')와 리다이렉션 기능, 그리고 백그라운드 실행을 결합해 보세요.
 - 탭(Tab) 자동 완성 기능이나 커맨드라인 문법 강조(Syntax Highlighting) 기능을 
   추가해 보세요.
 - Windows API(CreateProcess 등)를 사용해 완벽한 네이티브 윈도우 버전으로 포팅해 보세요.

얼마나 많은 학생들이 이 코드를 가지고 놀지 확실히는 모르겠습니다. 
하지만 누군가 이 코드를 기반으로 의미 있는 프로젝트를 진행한다면, 
우리 커뮤니티가 협력하는 모습을 볼 수 있어 정말 멋질 것 같습니다. 
초기 프로젝트들은 아마도 고립된 상태에서 거칠게 해킹된 형태겠지만, 
내년쯤에는 여러 플랫폼을 지원하고 훨씬 더 발전된 TUK-OS의 후속 버전이 
인터넷에 릴리즈되는 것을 볼 수 있다면 아주 기쁠 것입니다.

재밌게 즐기시길.



You still need the real campus data files in the 
'data/campus' directory to work with this code.

Many thanks to my team programmer for taking the time to grind through 
the 7 stages, clean up the project, and make sure that it actually works. 
Projects tend to rot if you leave them alone for a while, and it takes 
effort for someone to deal with it again.

The bad news: this code only compiles and runs on POSIX systems (Linux, 
macOS, WSL). We couldn't make it native for Windows because it relies 
heavily on standard Unix system calls like fork(), execvp(), and waitpid().

Still, the code is quite clean and written in standard C11, so it should be 
straightforward to bring it up on just about any Unix-like platform.

I wrote this code step-by-step, and there are plenty of things that seem 
downright silly in retrospect (using linear searches through flat, pipe-
separated text files for campus data comes to mind), but overall it should 
still be a useful base to experiment and build on.

The basic pipeline concept -- parsing tokens, executing built-ins or 
external binaries, and tracking background jobs -- was dead-on, but the 
implementation could be improved dramatically from the original code if 
it were revisited. The way the background jobs are tracked by sweeping 
with waitpid(WNOHANG) on every single REPL cycle could be replaced by a 
proper asynchronous SIGCHLD signal handler. It requires dealing with 
concurrency and reentrant functions, but it would be The Right Thing.

Blocking out real pipes ('|') and redirection ('>', '<') in this stage 
is one of the bigger misses that I look back on. I used a simple array 
for tokens, but I didn't realize at the time that building an AST 
(Abstract Syntax Tree) during parsing would have made implementing 
redirection vastly simpler.

Some project ideas:

Replace the local text files with a real REST API integration to fetch 
live campus data.

Add real rendering features to the terminal -- Tab auto-completion, 
command syntax highlighting, or look up/down history with arrow keys.

Implement real pipes and I/O redirection.

Port it to your favorite operating system (e.g., rewriting process 
spawning with Windows API).

I don't have a real good guess at how many students are going to be 
playing with this, but if significant projects are undertaken, it would 
be cool to see a level of community cooperation. I know that most early 
projects are going to be rough hacks done in isolation, but I would be 
very pleased to see a coordinated 'net release of an improved, backwards-
compatible version of TUK-OS on multiple platforms next year.

Have fun.

[원종우/Jongwoo0101]
07-16-2026

Copyright (c) 2026 [ClbeeQuest].