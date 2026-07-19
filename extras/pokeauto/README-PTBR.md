# PokeAuto integrado ao RAVBA

Esta modificação injeta os comandos diretamente no joypad emulado do RAVBA.
Ela não usa ViGEm, XInput ou controle virtual do Windows, portanto Steam e
outros aplicativos não recebem os botões.

## Uso

1. Abra Pokemon FireRed com código `BPRE`, revisão 1.
2. Use um save feito antes de escolher o inicial, com o personagem em frente à Poké Bola.
3. No RAVBA, abra `Input > PokeAuto (educational)`.
4. Clique em `Start FireRed starter hunt (10x)`.
5. O RAVBA repetirá a sequência integrada e lerá PID/OTID diretamente da EWRAM.
6. Quando encontrar um shiny, a automação para e grava `PokeAuto-found.txt`.

O teclado e o controle físico continuam misturados normalmente ao RetroPad.
A automação continua executando quando a janela perde o foco porque a entrada é
aplicada dentro do emulador.

A integração do RetroAchievements não foi alterada. O reset do PokeAuto usa o
mesmo comando interno de reset do menu, incluindo a chamada normal `RA_OnReset()`.
