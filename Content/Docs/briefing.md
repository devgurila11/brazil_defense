# Briefing atual — Brazil Defense

**Versão: 2026-10-05 11:45**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Trocar lados do HUD + mira das defesas sem prioridade de candidato

## 1. HUD dos votos — inverter os lados (está errado)

- Hoje o HUD do placar está com os lados AZUL e VERMELHO trocados.
- Inverter: trocar a posição dos dois lados — o número, o ícone de
  cédula e a direção que a barra de apuração enche, de cada lado.
- (Confirmar a orientação atual lendo o código e inverter para o lado
  certo. O usuário confirmou que está trocado.)

## 2. Mira das torres e personagens de plataforma — SEM prioridade de candidato

Observado: quando o candidato estava sendo alvejado e um militante
passou na frente (mais perto da defesa), a defesa CONTINUOU grudada no
candidato em vez de mirar o militante colado. Isso está ERRADO.

Correto (confirmado): torres e personagens de plataforma miram
PURAMENTE o mais próximo/mais adiantado no alcance, trocando
dinamicamente quando um NPC passa na frente — INCLUSIVE se o alvo atual
for o candidato. Sem prioridade especial para o candidato neste tipo
de defesa.

- REMOVER a prioridade do candidato nas torres e nos personagens de
  plataforma. O candidato é tratado como qualquer inimigo no alcance:
  se ele é o mais próximo/adiantado, leva tiro; se um militante passa
  na frente, o tiro vai para o militante.
- A regra de troca dinâmica (reavaliar a cada tiro, trava de margem)
  continua valendo — agora sem a exceção do candidato.

### O AGENTE é exceção (mantém)
- O Agente CONTINUA perseguindo o candidato (é o caçador dele, papel
  próprio). A remoção da prioridade vale só para torres e personagens
  de plataforma, NÃO para o Agente.

## 3. BUG — plataformas já vêm com 1 estrela cheia no nível 0

- Observado só nos PLATAFORMAS (o palanque mostra 1 estrela cheia
  mesmo sem evolução, nível 0). As torres parecem corretas.
- Causa provável: a plataforma evolui em BLOCO (o nível dela reflete o
  nível dos personagens nela). O cálculo deve estar tratando
  "lotada mas não evoluída" como nível 1, ou contando os personagens
  presentes como se fossem uma evolução — por isso 1 estrela cheia no
  que deveria ser nível 0.
- Correto: plataforma recém-construída / com personagens nível 1 base
  = nível 0 de evolução = 0 estrelas cheias (5 vazias). As estrelas só
  enchem conforme o BLOCO sobe de nível (evolução comprada), 1 por
  nível até 5.
- Verificar os TRÊS tipos de plataforma: palanque, caminhão e
  arquibancada — todos devem começar com 0 cheias.
- Confirmar que as torres já estão corretas (0 cheias no nível base);
  se não estiverem, alinhar.

## 4. Agente — movimento livre no campo (não preso à grade)

O Agente hoje se locomove só em 90° (frente, trás, esquerda, direita),
saltando de centro de célula em centro de célula — robótico.

Correto: o Agente anda LIVREMENTE por todo o campo, em ESPAÇO
CONTÍNUO, pisando em QUALQUER ponto e indo em QUALQUER ângulo. Ele NÃO
deve obedecer à grade no movimento — a grade (células) serve para a
lógica do jogo (rota da horda, posicionar peças), mas o Agente a
IGNORA ao se mover. Ele vagueia pelo terreno como uma pessoa de
verdade, não pelas linhas/centros da grade.

- Patrulha sem rota fixa: escolhe um ponto aleatório no campo e vai em
  linha reta até lá (qualquer ângulo, qualquer posição), para, escolhe
  outro, etc.
- Vira o CORPO suavemente para a direção do movimento (encara para onde
  anda) — não deslizar de lado nem virar seco.
- Mantém: sem colisão com construções (só separador bloqueia); raio de
  tiro; barra de tempo; etc.
- O ponto-chave: desacoplar o movimento do Agente da grade. Posição
  contínua no mundo, não célula a célula.

## Entregável
- HUD com azul/vermelho nos lados corretos (invertido).
- Torres e personagens de plataforma miram o mais próximo/adiantado,
  sem prioridade de candidato; trocam quando alguém passa na frente.
- Agente mantém a perseguição ao candidato.
- Plataformas (palanque/caminhão/arquibancada) começam com 0 estrelas
  cheias no nível 0; enchem só ao evoluir em bloco.
- Agente anda livre no campo, em espaço contínuo (não preso à grade/
  células), em qualquer ângulo, virando o corpo para onde anda.
- BD.Test.Regression passa; compilar os dois alvos; commit.
