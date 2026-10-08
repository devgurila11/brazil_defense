# Briefing atual — Brazil Defense

**Versão: 2026-10-08 15:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# BUG — células livres recusando colocação (preview vermelho)

Existem células do grid que estão LIVRES (nada posicionado) mas o
preview de colocação as mostra como VERMELHO (recusadas), como se
estivessem ocupadas. Isso não pode acontecer.

## Varrer e achar a causa (não é o usuário que inspeciona célula a célula)

Varrer o grid inteiro por script e encontrar onde o estado LÓGICO
diverge do que deveria estar livre:

1. Varredura de consistência: para cada célula, comparar o estado
   registrado no UBDGridSubsystem (Free/Tower/Platform/Divider/
   Blocked/Spawn/Goal) com o que REALMENTE ocupa aquela célula (há um
   ator posicionado ali? uma aresta? nada?). Listar as células cujo
   estado registrado diz "ocupada/bloqueada" mas NÃO há nada
   ocupando-as de fato. Essas são as fantasmas.

2. Causas prováveis a checar (reportar qual é):
   - RESÍDUO DE REMOÇÃO: peça vendida/movida que não liberou a célula
     de volta para Free. Especialmente peças 2×2 (palácio, plataformas)
     — conferir se a liberação das 4 células funciona.
   - CÉLULAS 2×2: construir/vender/mover palácio e plataformas marca e
     desmarca TODAS as células certas?
   - URNA SÓ-FRENTE: a mudança recente (urna aceita só pela frente)
     marcou células demais como bloqueadas? Conferir as células ao
     redor da urna.
   - SPAWN/GOAL: células de spawn/goal que ficaram marcadas após o
     ônibus/boca se mover, ou após a urna ser reposicionada.
   - AUTORAÇÃO: o layout salvo tem células marcadas erradas de origem?

3. Determinístico: rodar a varredura em vários tabuleiros sorteados
   (seeds) para ver se é sempre nas mesmas células (autoração/layout)
   ou após ações (resíduo de remoção).

## Corrigir a raiz
- Depois de achar: corrigir o ponto que deixa a célula marcada sem
  ocupante (a liberação que falha, o off-by-one, etc.).
- Garantir que vender/mover/remover QUALQUER peça devolve TODAS as
  células dela para Free.

## Blindar no regression
- Adicionar invariante: após construir e depois VENDER/MOVER cada tipo
  de peça (incluindo 2×2), todas as células dela voltam a Free — nenhuma
  célula fantasma. Rodar para palácio, plataformas, torre, separador.
- Invariante de consistência: nenhuma célula marcada como ocupada sem
  um ator/aresta real ocupando-a.

## Comando útil
- Se ajudar, um BD.Grid.Audit que roda a varredura e lista as células
  fantasmas (estado ocupado sem ocupante real), para diagnóstico rápido
  no futuro.

## Entregável
- Causa das células fantasmas encontrada e corrigida na raiz.
- Vender/mover/remover libera todas as células (inclusive 2×2).
- Regression com invariante de consistência de células.
- Compilar os dois alvos; commit.
