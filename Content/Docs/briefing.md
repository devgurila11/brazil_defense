# Briefing atual — Brazil Defense

**Versão: 2026-10-07 10:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Postes de iluminação acendendo à noite

Os postes já estão no cenário, com os SPOTS posicionados nos pontos de
acendimento. Falta fazê-los ACENDER à noite e APAGAR de dia, seguindo
o ciclo dia/noite que já existe. O usuário vai posicionar VÁRIAS CÓPIAS
do poste no terreno — então a solução tem que funcionar sozinha em
qualquer cópia, sem configurar uma a uma.

## Verificar o que já existe primeiro
- Lá no início do projeto foi desenhado um UBDStreetLightComponent que
  receberia a intensidade da curva CF_StreetLightIntensity do ciclo.
  Checar se esse componente/curva já existe no código e reaproveitar.
- O ciclo dia/noite já tem o CycleAlpha (UBDDayCycleComponent) e as
  curvas CF_Sun*. Usar a mesma fonte de tempo.

## O que fazer
- O poste deve ACENDER os spots ao entardecer e APAGAR ao amanhecer,
  com transição SUAVE (fade de intensidade), seguindo a curva do ciclo
  — não ligar/desligar seco.
- A intensidade dos spots acompanha a fase do dia: zero de dia, subindo
  no pôr do sol / fim de tarde, cheia à noite, caindo no nascer do sol.
- Se já houver a curva CF_StreetLightIntensity, usar ela; se não,
  criar (ou derivar do inverso da intensidade do sol).

## IMPORTANTE: escalar para várias cópias (preferir CÓDIGO)
- O usuário vai posicionar VÁRIAS cópias do poste. A lógica de acender
  deve estar de forma que QUALQUER cópia colocada no terreno já acenda
  sozinha, sem configuração manual por cópia.
- Preferir a lógica num COMPONENTE C++ (ou num ator de poste do jogo)
  que lê o ciclo e controla os spots. Assim, toda cópia do mesmo
  ator/componente acende automaticamente.
- Decisão BP vs código fica com o terminal, MAS o critério é: a solução
  tem que ser "posicionou, acende" para N cópias, sem trabalho manual.
  Se BP resolver isso igualmente bem, tudo bem; o importante é escalar.
- Confirmar como o usuário deve posicionar as próximas cópias (duplicar
  qual ator?) para que acendam.

## Performance
- Vários spots dinâmicos acendendo podem pesar. Avaliar: os spots
  precisam projetar sombra dinâmica (caro) ou só iluminar (mais leve)?
  Para horda com muitos postes, luz sem sombra dinâmica por poste
  costuma bastar. Deixar ajustável.

## Entregável
- Postes acendem à noite / apagam de dia, com fade, pelo ciclo.
- Qualquer cópia nova do poste acende sozinha (sem config por cópia).
- Instrução de como duplicar/posicionar as próximas cópias.
- BD.Test.Regression passa; compilar os dois alvos; commit.
- O usuário confere na tela o acendimento ao virar a noite (lembrar:
  a noite foi reduzida no ciclo; usar BD.Day.SetAlpha se houver, para
  forçar a noite e testar sem esperar).
