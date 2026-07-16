TUK-OS의 PowerShell 호환 코어(PowerShell Part) 소스 코드를 공개합니다.
ZSH 파트가 화려한 UI를, BASH 파트가 유닉스의 원시적인 시스템 콜을 다뤘다면, 이 디렉토리의 
코드는 평범한 텍스트 스트림을 넘어 '구조화된 객체(Object)'를 파이프라인으로 넘기려는 
야심 찬 시도를 담고 있습니다.

이 객체 지향적인 쉘의 뼈대를 세우느라 고생한 동료에게 감사를 전합니다. 프로젝트는 
조금만 방치해도 금세 썩어버리기 마련인데, 완전히 다른 철학을 가진 쉘 아키텍처를 
밑바닥부터 다시 짜맞추는 것은 꽤나 고된 작업이었으니까요.

나쁜 소식: 이 코드는 거대한 .NET 프레임워크나 CLR(공통 언어 런타임) 위에서 돌아가는 
진짜 Windows PowerShell이 아닙니다. 복잡한 COM 객체 바인딩이나 깊은 Windows 레지스트리 
훅(Hook), 그리고 완벽한 타입 리플렉션 같은 무거운 기능들은 과감히 쳐냈습니다.

이 코드를 작성하던 때를 돌아보면, 정말 멍청하게 느껴지는 부분들이 있습니다. 유닉스처럼 
단순히 표준 입출력(Raw Text)을 파이프로 넘기는 대신, 구조화된 데이터를 프로세스 간에 
넘기려다 보니 메모리 포인터와 직렬화를 하드코딩하며 골머리를 앓았던 기억이 납니다. 
명령어 파이프라인을 통과할 때마다 JSON 같은 가벼운 표준 규격으로 객체를 직렬화/역직렬화하는 
방식을 처음부터 채택하는 것이 '올바른 길(The Right Thing)'이었을 것입니다.

그래도 Cmdlet 특유의 '동사-명사(Verb-Noun)' 네이밍 규칙과 Alias(별칭) 시스템의 뼈대는 
꽤 정확하게 잡혀 있으며, 텍스트가 아닌 데이터를 다루는 쉘을 실험하기엔 훌륭한 장난감이 될 것입니다.

몇 가지 프로젝트 아이디어를 제안합니다:

 - 파이프라인('|')을 통해 단순 문자열이 아닌 JSON 형태의 객체를 주고받도록 구조를 
   완전히 뜯어고쳐 보세요.
 - 'ls'를 'Get-ChildItem'으로 매핑하는 등, PowerShell 특유의 강력한 Alias 시스템을 
   완벽하게 구현해 보세요.
 - 시스템 프로세스나 파일 정보를 윈도우의 WMI처럼 객체화하여, 텍스트 파싱(grep/awk) 
   대신 속성(Property) 단위로 필터링하는 기능을 추가해 보세요.
 - 진정한 크로스 플랫폼(PowerShell Core)을 지향하여 Linux/macOS 환경에서도 
   이 객체 지향 쉘이 네이티브처럼 돌게 만들어 보세요.

얼마나 많은 사람들이 이 PowerShell 파트까지 깊게 파고들지는 모르겠습니다. 하지만 누군가가 
이 코드를 바탕으로 객체 파이프라인을 완벽하게 구현해 낸다면 정말 쿨할 것입니다. 
내년쯤에는 커뮤니티의 힘을 빌려 훨씬 빠르고 강력한 TUK-OS PowerShell 클론이 
다양한 플랫폼에 릴리즈 되기를 바랍니다.

재밌게 즐기시길.

Here it is. The TUK-OS PowerShell compatible core (PowerShell Part) source 
code is released for your use. While the ZSH part focused on flashy UIs and 
the BASH part wrestled with raw Unix system calls, the code in this directory 
tackles an ambitious attempt to pass structured 'objects' through the 
pipeline instead of plain text streams.

Many thanks to my AI pair programmer for pushing through the nightmare of 
building this object-oriented shell skeleton. Projects tend to rot if you 
leave them alone for a while, and grafting a shell with a completely different 
philosophy onto our existing pipeline architecture was no small feat.

The bad news: this code isn't the real Windows PowerShell running on a massive 
.NET CLR. We stripped out the heavy stuff—complex COM object binding, deep 
Windows Registry hooks, and full .NET type reflection.

I wrote this code step-by-step, and there are plenty of things that seem 
downright silly in retrospect. Trying to manually pass and parse C-structs 
between processes to mimic an object pipeline was a huge headache. Adopting 
a standardized serialization format (like JSON) to pass structured data back 
and forth through the pipes from the very beginning would have been 
The Right Thing.

Still, the foundation for the classic 'Verb-Noun' Cmdlet naming convention 
and the Alias mapping system is dead-on. It should be a highly useful base 
to experiment with a shell that treats data as objects rather than raw strings.

Some project ideas:

Rewrite the pipeline logic to pass JSON-serialized objects between commands 
instead of raw character streams.

Implement a robust Alias system (mapping 'ls' to 'Get-ChildItem', etc.) 
that PowerShell is famous for.

Add property-based filtering so you can query file or process objects by 
their attributes, just like WMI, instead of relying on grep or awk.

Make it truly cross-platform (like PowerShell Core), ensuring this object-
oriented shell runs flawlessly on Linux and macOS natively.

I don't have a real good guess at how many people are going to dig this deep 
into the PowerShell part, but if someone builds a rough hack that perfectly 
handles object pipelines, that would be incredibly cool. I would be very 
pleased to see a coordinated 'net release of a smaller, faster, and more 
complete TUK-OS PowerShell clone on multiple platforms next year.

Have fun.

Copyright (c) 2026 [ClbeeQuest].