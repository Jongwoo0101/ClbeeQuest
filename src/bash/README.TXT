TUK-OS의 BASH 호환 코어(Bash Part) 소스 코드입니다.
ZSH 파트가 화려한 UI와 캠퍼스 기능에 집중했다면, 이 디렉토리의 코드는 리눅스 환경의 
날것 그대로인 표준 입출력과 시스템 콜을 제어하는 데 초점을 맞추고 있습니다.

ZSH 파트를 완성한 직후, 곧바로 BASH 특유의 악명 높은 파싱 규칙을 뜯어고치느라 
수고해 준 동료에게 다시 한번 감사를 전합니다. 이 바닥의 코드들은 한 달만 
들여다보지 않아도 금세 레거시가 되어버리니까요.

나쁜 소식: 이 코드는 완벽한 GNU Bash가 아닙니다. 복잡한 쉘 스크립트 제어문(if/while)이나, 
중괄호 확장(Brace Expansion), 연관 배열 같은 무거운 기능들은 덜어냈습니다. 
솔직히 말해서, 100% POSIX 호환성을 맞추는 건 이 프로젝트의 범위를 한참 벗어나는 일입니다.

이 코드를 짜면서 돌이켜보면, 정말 무식하게 접근했던 부분들이 눈에 밟힙니다. 
Bash의 그 까다로운 '단어 분리(Word Splitting)'와 다중 따옴표(Quoting) 처리 규칙을 
추상화 없이 C 언어의 포인터 연산만으로 하드코딩하려 했던 건 꽤나 바보 같은 짓이었습니다. 
명령어 파이프라인을 구축할 때 렉서(Lexer)와 파서(Parser) 단계를 명확히 분리하고, 
추상 구문 트리(AST)를 도입하는 것이 '올바른 길(The Right Thing)'이었을 것입니다.

그래도 기본적인 프로세스 분기(fork)와 실행(execvp) 메커니즘은 꽤 훌륭하게 작동하며, 
운영체제의 기본기를 실험하고 확장하기에는 훌륭한 베이스캠프가 될 것입니다.

몇 가지 프로젝트 아이디어를 제안합니다:

 - 파이프('|')와 I/O 리다이렉션('>', '<', '>>')을 완벽하게 구현해 보세요. 파일 디스크립터와 
   dup2()를 다루면서 골머리를 앓아보는 건 최고의 시스템 프로그래밍 공부가 될 겁니다.
 - 쉘 스크립트 파일(.sh)을 인자로 받아 일괄 실행하는 비대화형(Non-interactive) 
   모드를 만들어 보세요.
 - 환경 변수($VAR) 치환 기능을 정규식을 활용해 깔끔하게 재작성해 보세요.
 - 히스토리 확장(!!, !$) 기능을 추가해 보세요.

이 서브 프로젝트를 얼마나 많은 사람들이 뜯어볼지는 모르겠습니다. 
하지만 누군가가 이 BASH 파트를 기반으로 파이프라인 처리를 완벽하게 구현한 
거친 해킹 결과물을 만들어 낸다면 정말 흥미로울 것입니다. 내년쯤에는 커뮤니티의 
손을 거쳐 더 작고, 더 빠르고, 더 완벽한 TUK-OS Bash 클론이 릴리즈 되기를 기대합니다.

재밌게 즐기시길.

Here it is. The TUK-OS (BASH Part) source code is released for your use.
While the ZSH part focused on flashy terminal UIs and campus-specific features, 
the code in this directory gets back to the raw, bare-metal roots of Linux 
system calls and standard I/O streams.

Many thanks to my team programmer for jumping straight from the ZSH build 
into dealing with the notorious Bash parsing rules. Code tends to rot if you 
leave it alone for a few months, and it takes real effort to refactor it.

The bad news: this code is not a fully compliant GNU Bash. We stripped out 
the heavy stuff—complex shell script control structures (if/while), brace 
expansion, and associative arrays. To be completely honest, achieving 100% 
POSIX compliance was way beyond the scope of this project.

I wrote this code step-by-step, and there are plenty of things that seem 
downright silly in retrospect. Trying to hardcode Bash’s crazy word splitting 
and quoting rules using raw C pointer arithmetic without proper abstraction 
was a mistake. Separating the lexer and parser stages and building a proper 
Abstract Syntax Tree (AST) for the command pipeline would have been 
The Right Thing.

Still, the core fork-and-exec mechanism is dead-on, and overall it should 
still be a highly useful base to experiment with OS fundamentals.

Some project ideas:

Implement real pipes ('|') and I/O redirection ('>', '<', '>>'). Wrestling 
with file descriptors and dup2() is probably the best systems programming 
exercise you can get.

Add a non-interactive mode that takes a shell script (.sh) file as an 
argument and executes it batch-style.

Rewrite the environment variable expansion ($VAR) logic cleanly using regex.

Add history expansion features (!!, !$).

I don't have a real good guess at how many people are going to be playing 
with this specific bash directory, but if someone builds a rough hack that 
perfectly handles multi-stage pipelines, that would be incredibly cool. 
I would be very pleased to see a coordinated 'net release of a smaller, 
faster, and more complete TUK-OS Bash clone next year.

Have fun.

07-16-2026

Copyright (c) 2026 [ClbeeQuest].