# Briefing atual — Brazil Defense

**Versão: 2026-10-07 11:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# BUG — poste (BD Street Lamp) fica aceso continuamente

O poste em cena é o BD Street Lamp NOVO (o ator do jogo, na posição que
o terminal colocou — não o antigo montado à mão). Ele deveria acender
só à NOITE e apagar de dia, seguindo o ciclo. Mas está ACESO
CONTINUAMENTE, de dia e de noite.

Investigar e corrigir:
- O poste está lendo o CycleAlpha do ciclo dia/noite em runtime? Ou
  ficou com a intensidade fixa (sempre ligada)?
- A curva de intensidade por fase está sendo aplicada? De dia (alpha
  ~0.3) deveria ser 0 cd (apagado); de noite (alpha ~0.7) 10 cd (aceso).
- Pode ser que o componente leia o ciclo só no BeginPlay e não atualize
  a cada mudança de hora — então fica preso no estado inicial (que, se
  nasceu de noite ou com valor cheio, fica sempre aceso).
- A regressão testou 10 cd à noite e 0 de dia e passou — mas no jogo
  real fica sempre aceso. Ver a diferença entre o teste e o runtime:
  o teste força o estado; no jogo o poste precisa ACOMPANHAR o ciclo
  continuamente (Tick ou evento de mudança de fase), com fade suave.

Corrigir: o poste acende/apaga acompanhando o ciclo em tempo real,
apagado de dia, aceso de noite, com transição suave. Vale para todas as
cópias.

Testar: BD.Day.SetAlpha 0.3 (dia, apagado) → 0.5 (anoitecer, acendendo)
→ 0.7 (noite, aceso). O poste deve mudar junto.

## Entregável
- Poste acende só à noite, apaga de dia, acompanhando o ciclo em tempo
  real (não preso ao estado inicial).
- Todas as cópias seguem o mesmo comportamento.
- BD.Test.Regression passa (ajustar o teste se ele não pega o caso do
  runtime contínuo); compilar os dois alvos; commit.
