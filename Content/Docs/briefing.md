# Briefing atual — Brazil Defense

**Versão: 2026-10-02 15:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Ajustes do Agente + som das falas + debug on-click

Vários ajustes, todos verificados em teste. Agrupados por tema.

---

## A) AGENTE — comportamento

### A1. Voto por CORPO (muda de HP para contagem)
- Hoje o voto é por HP do inimigo (gera decimais, não bate com abates).
  Mudar para 1 voto por CORPO:
  - Militante morto pelo jogador = +1 voto AZUL.
  - Militante que chega na urna = +1 voto VERMELHO.
  - Candidato: NÃO dá voto (nem azul ao morrer, nem conta como corpo
    normal). Candidato morto solta PROPINA (como já é). Candidato na
    urna = derrota (como já é).
- Isso faz o placar bater com os abates e acaba com os decimais.
- Expor se algum ajuste de balanceamento depende disso (a escala do
  placar muda de HP-somado para contagem).

### A2. Agente persegue o candidato
- NPC comum (militante): o Agente PARA e atira, NÃO persegue (como já é).
- CANDIDATO: o Agente PERSEGUE e atira até matar ou até a barra
  esgotar. O candidato é o inimigo nº1 do Agente. Ele larga a patrulha
  e vai atrás do candidato quando um está em campo.

### A3. Atira até deitar (corrigir)
- Hoje o Agente fica inofensivo no caminho de volta ao palácio. BUG.
- Correto: ele atira em quem entra no raio o tempo todo que está
  ACORDADO — patrulhando E no trajeto de volta. Só desarma quando
  DEITA para dormir.

### A4. Acordar pelo TEMPO, com barra enchendo (corrigir)
- Hoje ele só acorda quando a onda em que dormiu acaba. BUG.
- Correto: o descanso é um TEMPO (relógio) que corre e termina
  sozinho, acordando o Agente mesmo no meio de uma onda.
- Durante o sono, a BARRA (a mesma de tempo) ENCHE, mostrando o
  progresso até acordar. (Patrulhando ela esvazia; dormindo ela enche.)

### A5. Duração do descanso — EFICIÊNCIA PREMIA (regra B)
- Quanto MAIS abates o Agente fez na patrulha, MENOS ele descansa
  (volta rápido para a ação — recompensa o bom desempenho).
- Fórmula sugerida: descanso = DescansoBase - (abates × ReducaoPorAbate),
  com um piso mínimo (ex: base 45s, -0,5s por abate, mínimo 10s).
  Expor DescansoBase, ReducaoPorAbate e DescansoMin em settings.
- Confirmar os números em teste; o importante é a regra: matou muito,
  descansa pouco.

### A6. Mira dinâmica (armas de disparo)
- Armas de DISPARO (pistola e torres atuais): a cada tiro, reavaliar o
  alvo — mirar o NPC mais próximo naquele instante. Trocar de alvo se
  um novo chegou mais perto, MESMO que o anterior não tenha morrido.
- Trava anti-indecisão: só troca se o novo estiver SENSIVELMENTE mais
  perto (margem ~15-20%), para não ficar gaguejando entre dois quase
  à mesma distância.
- O indicador (line trace) acompanha: mostra quem É o alvo atual.
- Armas CONTÍNUAS (laser, FUTURO): mira GRUDADA no alvo (dano crescente
  premia manter). Não é agora, mas deixar claro que o tipo de mira é
  característica da arma (disparo = dinâmica; contínua = grudada).

### A7. BUG — sincronia do tiro
- 1º e 2º tiro sincronizam com a animação Shooting; do 3º em diante o
  braço dá o tranco mas o tiro não sai ou sai fora do tempo.
- Causa provável: a cadência (timer) e a animação derrapam. Amarrar o
  DISPARO ao FRAME do tiro na animação (Anim Notify no Shooting), não a
  um timer separado — cada tiro sai quando o gesto acontece, sempre em
  sincronia.

### Mantido (não mexer)
- Colisão livre: o Agente atravessa tudo, só o separador de fila o
  bloqueia. Fica como está (há jogos assim; evita ele prender num
  canto cercado).

---

## B) SOM DAS FALAS (só falas, não efeitos)

- Hoje até ~4 falas tocam juntas e se atropelam — não dá para entender.
- Nova regra SÓ para as FALAS (relincho/casco/efeitos ficam como estão):
  - Teto de 2 falas simultâneas.
  - As 2 só tocam juntas se vierem de LADOS SEPARADOS em relação à
    câmera (separação de posição/ângulo mínima). Se duas quminam querem
    falar do mesmo lado, uma espera.
  - Espacialização 3D forte (stereo spread) para distinguir
    esquerda/direita — a graça é perceber a fala vindo de direções
    diferentes, em ouvidos diferentes (no fone/estéreo).
  - Prioridade: as 2 falas mais próximas da câmera, em lados diferentes.
- Efeitos (relincho, casco) mantêm o teto maior atual, sem essa regra.

---

## C) DEBUG DE ALCANCE — on-click (opção A)

- TIRAR todos os debug spheres permanentes (alcance de torre, raio,
  etc.) do campo visual padrão. Nada de círculo sempre ligado — polui.
- O alcance passa a aparecer ON-CLICK, pelo próprio clique de SELEÇÃO:
  - Clicar numa peça (torre, plataforma, Agente) SELECIONA ela E mostra
    o alcance/raio dela.
  - Clicar em outra peça troca para o alcance dela.
  - Clicar no vazio (desselecionar) esconde.
- Vale para torre (alcance), plataforma (alcance dos atiradores) e
  Agente (raio de 3 células). Cada peça mostra o seu ao ser clicada.
- As piscadas de tiro do Agente (BD.Agent.ShowShots) continuam como
  estão (debug separado, por cvar).

---

## Entregável
- Voto por corpo (militante; candidato = propina, sem voto).
- Agente persegue candidato; para e atira em militante.
- Atira até deitar (inclusive na volta); acorda pelo tempo com barra
  enchendo; descanso menor quanto mais abates (regra B).
- Mira dinâmica para disparo (trava anti-gago); laser grudado fica p/
  futuro.
- Sincronia do tiro corrigida (Anim Notify).
- Falas: teto 2, só de lados separados, espacialização forte.
- Debug de alcance on-click (seleção mostra o raio), sem spheres
  permanentes.
- BD.Test.Regression passa; compilar os dois alvos; commit.
