# Briefing atual — Brazil Defense

**Versão: 2026-10-13 14:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Raio-X da mecânica atual — SOMENTE LEITURA

Vamos adicionar mecânicas novas (poderes de ministro e candidato,
magias do jogador, hordas especiais liberadas por candidato morto).
Antes disso precisamos de uma foto exata do que o jogo FAZ hoje, para
não quebrar o que está bom nem implementar coisa que já existe de outro
jeito.

**Nesta rodada não se modifica nada**: nenhum arquivo de código, asset,
config, PLANO ou briefing. Nenhum commit. Nenhum teste novo. Só ler,
rodar o que for preciso para confirmar um valor (headless, `Status`,
`Report`) e responder.

## Fonte da verdade

Código em C++ primeiro; depois os valores reais gravados nos DataAssets,
`DefaultGame.ini` e Project Settings; o PLANO §1–11 por último. Onde o
PLANO diz uma coisa e o código faz outra, a tabela mostra o que o
CÓDIGO faz e marca a divergência. Nunca descrever uma regra a partir do
PLANO sem confirmar no código.

## Produto: tabelas, uma por área

Cada linha = uma regra. Colunas fixas:

| Regra | Como funciona hoje | Parâmetro(s) e valor atual | Onde está (classe / asset / setting) | Diverge do PLANO? |

Valores sempre os que VALEM em jogo (asset sobrescreve default de
classe; se o asset não define, dizer "default de classe: X").

Áreas, nesta ordem:

1. **Loop da partida** — setup, montagem, onda, entre ondas, fim; o que
   trava quando; velocidades 1x/2x/4x; pausa.
2. **Economia** — votos azul/vermelho/nulo (por corpo ou por HP, o que
   vale hoje); dinheiro público (de onde entra, o que gasta); propina
   (quem solta, quanto, para onde vai); preço de cada peça; evolução
   (base, crescimento, níveis); venda; mover; cota de separador;
   `StartingFunds`/`StartingVotes` por dificuldade; desbloqueios por onda.
3. **Horda** — tipos existentes e seus números (HP, velocidade, escala
   por onda, crescimento); quantos por onda e por boca; bocas ativas;
   bocas móveis; ônibus; rota e variação; comportamento ao chegar na
   urna; morte (corpo, som, sangue).
4. **Candidato** — intervalo, HP, velocidade, boca, prioridade de mira
   (o que vale hoje: Mito sim, torres não?), o que acontece ao morrer
   (propina, ministro, ...), ao chegar (derrota), grupo no endless,
   barra/nome no HUD.
5. **Virada e retorno** — gatilho exato, quem volta, com quanta vida, em
   quanto tempo, o que a onda vira, o que acontece ao matar todos, sem
   caídos.
6. **Palácio e Mito** — custo, 2×2, níveis/estrelas, armas por nível;
   Mito: patrulha, alcance, mira (quem primeiro), tiro, barra de
   tempo/estamina, descanso, bônus por abate, perseguição, volta pra
   casa, chute, falas, tamanho.
7. **Ministro** — quando sai, quantos, de onde, passeio, refúgio
   (raio, quem protege, o que bloqueia), leitura e tempo pós-raio
   vazio, chute (distância, voo, queda, fuga), separadores, DA e campos.
8. **Peças de defesa** — torres, personagens de plataforma, plataformas
   (slots, altura, bloco), separadores (aresta, validação); mira de cada
   um (mais adiantado no alcance / mais próximo / trava); alcance por
   nível e por plataforma; cadência, pente, recarga; coice;
   reconhecimento; "todos condenados".
9. **Dificuldade, vitória, saves** — WavesToWin, presos, saves, bônus em
   cadeia, endless, o que decide vitória/derrota e em que ordem as
   condições são checadas.
10. **Ambientação que afeta jogo** — dia/noite (ondas por dia, fatia de
    noite), postes; áudio só o que tem regra de jogo (teto de falas,
    ducking) — sem detalhar mixagem.
11. **Elenco (seção 14)** — DAs por tipo, campos de cada classe de
    dados, quais estão ligados ao jogo hoje e quais só existem no asset.

## Tabelas extras

- **Settings e DataAssets**: lista de TODOS os `UBD*Settings` e `DA_*`
  com os campos que afetam gameplay e seus valores atuais. Uma tabela
  por classe. Campos de debug/visual podem ficar de fora.
- **Regras só no código**: tudo que o jogo faz e que NÃO está escrito
  em nenhuma seção do PLANO §1–11 (ex.: regra de espera dos ônibus,
  proteção `WouldCutOffCells`, `ReacquireGrace`...). Uma linha cada.
- **Divergências PLANO × código**: lista consolidada do que as tabelas
  marcaram, com a seção do PLANO e o que o código faz.
- **Comandos de console** que afetam ou inspecionam gameplay, com o que
  cada um faz (uma linha cada; os de debug visual podem ficar de fora).

## Formato da resposta

- Tudo na resposta do terminal, em Markdown, tabelas de verdade (não
  listas). Pode ser longa; não resumir para caber.
- Começar com um sumário de 10 linhas: as 10 regras que mais definem o
  jogo hoje, na opinião dele.
- Terminar com "Dúvidas que o código não responde": pontos onde a regra
  é ambígua ou depende de ordem de execução que ele não conseguiu
  confirmar.
- Não propor melhoria, não opinar sobre balanceamento, não antecipar as
  mecânicas novas. Só o que existe.

## Entregável

- A resposta acima. Nenhuma alteração no repositório (`git status`
  limpo ao final, igual ao início).
