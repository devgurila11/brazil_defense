# Briefing atual — Brazil Defense

**Versão: 2026-10-10 22:30**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# FATIA 3a: Ministro básico (cubo preto) — chute + falas

Antecipar o ministro como BLOCAGEM (cubo preto, como o candidato é
cubo) para testar o chute do Mito, as falas e a exclamação preta, sem
o modelo final. SÓ a fatia 3a — o básico. Refúgio/invisibilidade, os
11 limitados e a saída ao matar candidato são a 3b, NÃO fazer agora.

## O ministro (3a)
- Novo ator ABDMinister, placeholder CUBO PRETO. Já nasce DOBRADO de
  tamanho (como Mito/candidatos — figura especial, sobressai na horda).
- Sai do STF: o SM_STF já está em cena no lado OPOSTO aos ônibus, mas
  hoje é só o prédio, não solta ninguém. Fazer o STF SOLTAR os
  ministros (o respawn deles é a posição do STF).
- Patrulha livre pelo grid: vagueia em espaço contínuo (como o Agente),
  sem colisão com construções — só o separador de fila bloqueia.
- Exclamação PRETA ao falar (a cor preta já está mapeada).

## Chute do Mito no ministro
- O chute já existe (BD.Agent.Kick, animação Kicking, SCue_Mito_Chute).
  Ligar ao ministro de verdade:
  - O Mito PERSEGUE e chuta o ministro quando perto (como persegue o
    candidato).
  - Ao chutar: o ministro VOA para o alto, grita, cai no chão e FOGE de
    volta para o STF.
  - Dispara a fala de chute do Mito (SCue_Mito_Chute) + exclamação azul
    do Mito, e um grito/exclamação preta do ministro.
  - Ao chegar no STF, o ministro some (na 3a; a regra "só volta ao
    matar candidato" é 3b — aqui pode sumir ou reaparecer para testar).

## Comandos de teste
- BD.Minister.Spawn [n] — solta N ministros do STF.
- BD.Minister.Status — lista os ministros em campo.

## NÃO fazer agora (3b, depois)
- Os 11 limitados e identificados.
- Sair UM a cada candidato morto.
- Raio de 2 células = refúgio (NPC invisível/imune).
- Animação de raiva ao entrar, "só volta ao matar próximo candidato".
- A garrafa de whisky (fatia 4).

## Entregável
- ABDMinister cubo preto dobrado, soltado pelo STF (oposto aos ônibus).
- Patrulha livre (só separador bloqueia).
- Mito persegue e chuta: ministro voa, grita, cai, foge pro STF.
- Fala de chute do Mito + exclamação azul dele + grito/exclamação preta
  do ministro.
- Comandos Spawn/Status.
- BD.Test.Regression passa; compilar os dois alvos; commit.
