# Briefing atual — Brazil Defense

**Versão: 2026-10-10 21:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Conserto de áudio de fala + ducking + tamanho dos especiais

Três frentes nesta leva.

---

## A) ÁUDIO DE FALA — 3 correções que se somam

Diagnóstico confirmado: a Nicole fica inaudível e os militantes falam
demais. Corrigir as três:

### A1. Militantes falam menos conforme a horda cresce (NÃO foi feito)
- Hoje cada creep tenta falar a cada 8-20s sem olhar quantos há em
  campo (168 tentativas em 40s numa onda pequena; com 200, 10+/s).
- Corrigir: a chance de cada militante falar cai conforme a horda
  cresce, mirando um NÚMERO FIXO de tentativas/s no tabuleiro inteiro
  (alvo ~1 fala de militante a cada 2-3s no total, com 20 ou 200).
- Fórmula: chance ∝ (alvo / militantes_vivos), com piso. Expor o alvo.

### A2. Volume das falas da Nicole (+4 dB)
- As falas da Nicole são ~4,4 dB mais baixas que as do Mito/militantes
  (-21,0 vs -16,6 dBFS) — ficam encobertas.
- Normalizar as 17 falas da Nicole para o nível das outras (~+4 dB),
  reprocessando dos originais em SourceAudio. Pico abaixo de 0 dBFS.

### A3. Prioridade das falas dos personagens do jogador
- As falas dos SEUS personagens (Nicole, Mito) têm PRIORIDADE sobre as
  dos militantes no limite de 2 vagas: se as 2 vagas estão ocupadas por
  MILITANTES, o personagem toma a vaga (corta um militante); se por
  outro personagem seu, respeita. Já acontece no chute do Mito;
  estender para a comemoração.

---

## B) DUCKING — efeitos abaixam quando há fala

- Quando QUALQUER fala toca (militante, Nicole, Mito, futuros), os
  EFEITOS (tiro, relincho, casco, impacto, etc.) abaixam para ~55% do
  volume, para a fala se destacar.
- A MÚSICA NÃO abaixa (fica como está) — só os efeitos.
- Abaixa enquanto houver QUALQUER fala tocando; volta ao normal quando
  a última fala acaba, com transição SUAVE (~0,4s), não corte seco.
- Usar Sound Class / Sound Mix (o projeto já usa SC_Effects): a classe
  de fala aciona o ducking na classe de efeitos.
- Valores (duck para 55%, fade 0,4s) ajustáveis em settings.

---

## C) TAMANHO — dobrar os especiais (NÃO os militantes)

- DOBRAR a escala ATUAL de: Mito (Agente), Candidatos e Ministros.
  Eles devem SOBRESSAIR sobre a horda de militantes — figuras especiais
  visivelmente maiores.
- Militantes NÃO mudam (ficam na escala atual).
- Ao dobrar o Mito: garantir que TUDO acompanha a nova escala — a arma
  no socket (escala junto, empunhadura certa), a barra de patrulha
  (acima da cabeça nova), as animações, a exclamação, o raio/tiro.
  Conferir que nada fica flutuando ou no lugar errado.
- Candidatos e Ministros (cubos por enquanto): dobrar o cubo; quando os
  modelos reais entrarem, já saem no tamanho dobrado.
- A barra de vida e as marcas de exclamação desses especiais sobem para
  a altura da nova escala.

---

## Entregável
- Militantes falam menos com horda grande (alvo fixo/s).
- Falas da Nicole normalizadas (+4 dB).
- Prioridade das falas dos personagens do jogador.
- Ducking: efeitos a 55% quando há fala, música intacta, fade 0,4s.
- Mito, Candidatos e Ministros com o dobro do tamanho atual (militantes
  não); tudo acompanhando (arma, barras, exclamações).
- BD.Test.Regression passa; compilar os dois alvos; commit.
