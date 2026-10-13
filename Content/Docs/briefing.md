# Briefing atual — Brazil Defense

**Versão: 2026-10-13 17:30**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Bugs apontados pelo RAIOX — corrigir antes de mecânica nova

Primeiro: commitar o `Content/Docs/RAIOX.md` como está (entrada curta
no §12). Depois esta leva. Nada de mecânica nova aqui; só o que está
abaixo. Cada item ganha check na regressão. No fim, corrigir no RAIOX
as linhas que mudaram e a data do cabeçalho.

## 1. Contagem da onda 1 só começa com a urna no chão

Hoje a contagem da onda 1 corre sem urna; se zera, a onda sai, os
creeps são descartados e a partida fica presa. Regra: enquanto não há
urna no board, a contagem não anda (HUD mostra "coloque a urna" no
lugar do tempo) e `BD.Match.CallWave` recusa. A urna no chão dispara
a contagem com o `FirstWaveDelay` cheio. Ela continua travada a partir
da onda 1 como hoje.

Check: sem urna, 200 s de jogo não iniciam onda; urna posta → contagem
começa do valor do DA.

## 2. Ministros não são resetados no endless

Ministro só sai de campo pelo chute do Mito. Vitória na onda 100 →
"Continuar (endless)": os ministros de pé continuam exatamente como
estão (posição, estado, assento), e a fila de assentos também. Nenhum
reset.

Limpeza só quando o board é desfeito de qualquer forma: partida nova
(Play), rebobinar para onda 0, derrota seguida de load. Aí todos somem
e a fila volta a 1..11. Conferir que hoje não existe essa limpeza e
que o subsistema do ministro é zerado junto com o `ResetForNewMatch`.

Check: 3 ministros de pé → vitória → endless → os 3 continuam no mesmo
lugar e o próximo candidato morto solta o assento certo. Partida nova
→ zero ministros, fila 1..11.

## 3. Save/load não respeita as regras

Sintoma do usuário: o "Continue" / load traz um layout que foi criado
na época dos testes de save, como se fosse fixo, e a contagem de saves
não é respeitada. Investigar ANTES de corrigir — hipótese principal: a
regressão e o `BD.Save.Write` dos testes headless gravam no MESMO slot
`BDMatch` que o jogador usa, então o último teste rodado vira o
"Continue" do jogador. Confirmar lendo o `.sav` atual e o log da
regressão.

Regras a valer depois da correção (PLANO §8, com o que o RAIOX achou):

- Slot do jogador separado de qualquer slot de teste. Testes e
  regressão usam `BDMatch_Test` (ou gravam em memória) e NUNCA tocam
  no slot do jogador. Check que falha se tocarem.
- Um ponto de restauração por partida, sobrescrito a cada save, só em
  Building. `SaveBudget` desconta a cada save; a quantidade por
  dificuldade fica a que estiver nos DAs (o usuário decide 3/2/1 ou
  2/2/2 — não mudar nesta leva, só respeitar).
- O save pertence à partida: guarda dificuldade, seed e `MatchStart`.
  Load só é oferecido/aceito para a mesma dificuldade; um save de
  outra dificuldade não aparece no "Continue" daquela.
- Load não consome save (é o ponto de restauração), mas o slot é
  apagado quando a partida termina em vitória ou quando o jogador
  começa uma partida nova (Play). Derrota mantém o slot (é o caso de
  uso do painel de derrota).
- Load por console (`BD.Save.Load`) segue a mesma trava de fase e
  dificuldade que o botão.
- Restaurar o que hoje não restaura: posição das bocas, registros de
  candidatos (enviados, caídos, ordinal), `LevelDownCount` e o tempo
  restante da contagem. O load reabre na montagem da onda salva com
  o estado que ela tinha.

Checks: slot de teste nunca é o do jogador; save em WaveActive
recusado; load de outra dificuldade recusado; vitória apaga o slot;
derrota mantém; load repõe bocas e caídos iguais aos salvos.

## 4. Apuração e candidato pendente (dúvida #3)

`IsEndDue` ignora candidato no ônibus e candidato pendente
(`bSchedulePending`). Numa onda ≥ 100 a apuração pode correr antes de
ele sair. Regra: a apuração espera candidato vivo, no ônibus, pendente
e retorno em curso; só sem nenhum deles ela resolve.

Check: onda 100 com candidato pendente → sem apuração até ele cair ou
chegar.

## 5. Load com vermelho na frente (dúvida #6)

Como os caídos zeravam no load, o broadcast de votos podia soltar o
candidato antecipado já na montagem. Com o item 3 restaurando os
caídos isso deve sumir; conferir e cobrir com check: load de um save
com vermelho > azul e caídos registrados não solta candidato na
montagem.

## Regras da leva

- Regressão passando inteira antes de criar os checks novos, e depois
  com eles. Compilar os dois alvos. Um commit por item (ou dois se
  fizer sentido), entrada no §12.
- RAIOX atualizado: linhas de loop (urna), ministro (reset), saves
  (todas as de §9) e as dúvidas #1, #3, #6, #11 e #32 saem da tabela
  de dúvidas.
- Se o item 3 revelar outra causa que não o slot compartilhado,
  descrever a causa real antes de corrigir.
