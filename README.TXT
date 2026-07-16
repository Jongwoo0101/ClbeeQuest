이 코드가 제대로 작동하려면 `data/campus` 디렉토리에 실제 데이터 파일들이 있어야 합니다. 만약 원본 데이터가 없다면 형식을 맞춰서 직접 텍스트 파일을 작성해 넣으셔야 합니다.

프로젝트를 정리하고 제대로 돌아가게끔 다듬어준 팀원들에게 깊은 감사를 전합니다. 코드란 건 몇 달만 방치해도 금방 썩어버리기 마련이고, 그걸 다시 꺼내어 쓸만하게 고치는 데는 꽤 많은 노력이 필요합니다.

이 코드는 `fork()`와 `execvp()` 같은 유닉스 시스템 콜에 강하게 의존하여 작성되었습니다. 하지만 흥미롭게도 이 녀석은 윈도우 환경에서도 실행이 됩니다! 소스 코드 파일들이 제대로 갖춰져 있다면, 컴파일된 `.exe` 파일을 실행했을 때 아주 정상적으로 동작하는 것을 확인할 수 있을 것입니다. (물론 완전한 Windows API 기반의 네이티브 포팅을 원한다면 여전히 골치 아픈 작업이 남아있긴 합니다.)

그럼에도 불구하고 코드는 꽤 이식성이 높습니다. C11 표준만 지킨다면 어떤 유닉스 플랫폼이나 윈도우 호환 환경에서든 쉽게 구동할 수 있을 것입니다.

이 코드를 처음 짤 때를 돌이켜보면, 지금 와서 보기엔 정말 어이없을 정도로 비효율적으로 짠 부분들이 널려있습니다 (명령어를 입력할 때마다 파이프 구분자로 된 평문 텍스트 파일을 매번 처음부터 끝까지 선형 탐색하는 방식이 제일 먼저 떠오르네요). 하지만 전반적으로 이 녀석은 운영체제와 쉘의 기본기를 실험하고 발전시키기 위한 훌륭한 베이스캠프가 될 것입니다.

명령어를 토큰으로 쪼개고, fork로 분기한 뒤 exec로 덮어씌우는 기본 파이프라인의 개념 자체는 완벽했습니다. 하지만 나중에 코드를 다시 짤 기회가 생긴다면 현재 구조를 꽤 많이 갈아엎어야 할 것입니다. 백그라운드 작업(jobs)을 추적한답시고 루프가 돌 때마다 `waitpid(WNOHANG)`를 무차별적으로 호출하는 방식은 세련되지 못했습니다. 제대로 된 비동기 `SIGCHLD` 시그널 핸들러를 구현해서 처리하는 것이 '올바른 길(The Right Thing)'일 것입니다.

이번 버전에선 파이프(`|`)와 리다이렉션(`>`, `<`)을 명시적으로 막아두었는데, 돌이켜보면 이 역시 아쉬운 결정 중 하나입니다. 코드가 복잡해질까 봐 제외했지만, 토큰을 분리할 때 추상 구문 트리(AST)를 미리 구축해 두었더라면 파이프 처리 정도는 훨씬 수월했을 것입니다.

이 코드로 해볼 만한 프로젝트 아이디어 몇 가지를 제안합니다:

* 가장 선호하는 운영체제의 순수 네이티브 API로 포팅해 보십시오.
* 실제 파이프(`|`)와 I/O 리다이렉션을 구현해 보십시오.
* 단순한 로컬 텍스트 파일 파싱을 버리고, 실제 학교 서버 REST API를 연동해 보십시오.
* 파워쉘 파트를 건드리게 된다면, 텍스트 스트림 대신 진짜 JSON 객체를 파이프라인으로 넘기도록 구조를 뜯어고쳐 보십시오.

얼마나 많은 분들이 이 코드를 가지고 놀게 될지는 잘 모르겠습니다. 초기에는 다들 고립된 환경에서 투박하게 해킹한 엉성한 결과물들만 나오겠지만, 내년쯤에는 누군가 파이프라인과 비동기 처리를 완벽하게 개선하고 하위 호환성까지 유지되는 TUK-OS의 멀티플랫폼 버전을 인터넷에 릴리즈해 준다면 정말 기쁠 것 같습니다.

재밌게 즐기시길 바랍니다.


Here it is, at long last. The TUK-OS source code is released for your use.
You still need the real campus data in the `data/campus` directory to work with this code. If you don't actually have the original text files, you'll just have to fake it and make your own.

Many thanks to my team members for taking the time to clean up the project and make sure that it actually works. Projects tend to rot if you leave them alone for a few months, and it takes effort for someone to deal with it again.

We relied heavily on standard Unix system calls like `fork()` and `execvp()`. But surprisingly enough, it actually runs on Windows! As long as you have the source code files in place, running the compiled `.exe` file works normally. (Though, if you want a pure, ground-up Win32 API native port, that's still a completely different beast to tackle.)

Still, the code is quite portable, and as long as you stick to the C11 standard, it should be straightforward to bring it up on just about any Unix-like or Windows compatible environment.

I wrote this code a while ago, and there are plenty of things that seem downright silly in retrospect (using a linear search through flat, pipe-separated text files every single time a command is run comes to mind), but overall it should still be a useful base to experiment and build on.

The basic pipeline concept -- parsing tokens, branching with fork, and overlaying with exec -- was dead-on, but the implementation could be improved dramatically from the original code if it were revisited. The way the background jobs are tracked by sweeping with `waitpid(WNOHANG)` on every single REPL cycle is just brute force. Replacing it with a proper asynchronous `SIGCHLD` signal handler would be The Right Thing.

Blocking out real pipes (`|`) and redirection (`>`, `<`) in this stage is one of the bigger misses that I look back on. I kept them out to avoid messy code, but if I had built a proper Abstract Syntax Tree (AST) during parsing, dealing with pipes would have been pretty easy.

Some project ideas:

* Port it to your favorite operating system using pure native APIs.
* Implement real pipes (`|`) and I/O redirection.
* Ditch the raw local text files and integrate a real REST API for campus data.
* If you touch the PowerShell part, rewrite the pipeline to pass actual JSON objects instead of text streams.

I don't have a real good guess at how many people are going to be playing with this, but if significant projects are undertaken, it would be cool to see a level of community cooperation. I know that most early projects are going to be rough hacks done in isolation, but I would be very pleased to see a coordinated 'net release of an improved, backwards-compatible version of TUK-OS on multiple platforms next year.

Have fun.

[Jongwoo Won / Jongwoo0101]
07-16-2026

Copyright (c) 2026 [ClbeeQuest].
Licensed under the MIT License.