# Briefing atual — Brazil Defense

**Versão: 2026-09-24 23:45**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Votos iniciais (handicap de placar) por dificuldade

Hoje o StartingVotes é 3.000 nas TRÊS dificuldades — o azul começa
liderando por igual em Easy, Normal e Hard. Isso contradiz a ideia de
dificuldade: o handicap de largada deveria diminuir conforme sobe.

Efeito real desses votos (confirmado): atrasam a derrota por placar,
atrasam o gatilho do retorno dos caídos (vermelho passar o azul), e
somem no nivelamento pós-desfile. É um fôlego de começo.

## Fazer

Escalar o StartingVotes por dificuldade nos DA_Difficulty:

- Easy: 3.000 (perdoa bastante — para quem está aprendendo)
- Normal: 1.500 (metade — fôlego menor)
- Hard: 0 (sem fôlego — eleição começa 0 a 0, sem perdão)

O ChainBonus.Votes (+100 por ter vencido a dificuldade abaixo) fica
como está — é vantagem merecida por progressão, separada do handicap.
Então no Hard, quem venceu o Normal ainda começa com +100; quem entra
direto no Hard começa em 0.

O vermelho continua começando em 0 sempre (sem valor inicial).

## Nota

StartingFunds (a moeda, 2.000) NÃO muda — é o capital de montagem,
necessário nas três dificuldades. Só o StartingVotes (placar) escala.

## Entregável

- StartingVotes: Easy 3000, Normal 1500, Hard 0 nos três DA_Difficulty
  (gravar pelo commandlet, como das outras vezes).
- Confirmar lendo os três valores numa sessão nova.
- BD.Test.Regression passa.
- Compilar os DOIS alvos (editor e jogo), como combinado.
- Commit + push, hash no PLANO §12.
