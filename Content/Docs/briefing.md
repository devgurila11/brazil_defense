# Briefing atual — Brazil Defense

**Versão: 2026-10-12 10:00**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Áudio: 4 categorias no menu + ênfase nas falas

Reorganizar o áudio em 4 categorias com controle próprio, dando ÊNFASE
às FALAS (a graça do jogo) — elas não podem ficar abafadas por nada.

## 1. Quatro Sound Classes separadas, cada uma com slider + mudo

Hoje há basicamente Music e Effects (com Voice e ambiente dentro).
Separar em QUATRO, cada uma controlada por um slider próprio E um
checkbox de MUDO no menu Options:

- VOICES — todas as falas (militantes, Nicole, Mito, ministros futuros).
- MUSIC — a trilha.
- ENVIRONMENT — ambiência (cidade, pássaros, coruja, grilos).
- EFFECTS — tiros, casco, relincho, impacto, urna, motor, buzina, tombo.

Reorganizar as Sound Classes: SC_Voice, SC_Music, SC_Environment,
SC_Effects, independentes. Cada slider e cada mudo afeta só a sua.
Persistir no SaveGame de configurações, como já é.

## 2. Volumes default
- VOICES: 80% (destaque — a fala é a estrela).
- MUSIC: 20%.
- EFFECTS: 50%.
- ENVIRONMENT: 40%.
- São os valores iniciais (sem config salva); config do jogador é
  respeitada.

## 3. Ducking com ênfase nas falas
- Quando QUALQUER fala toca: EFFECTS e ENVIRONMENT abaixam (os dois
  cedem, para a fala sobressair sobre tudo).
- MUSIC fica CONSTANTE (não abaixa).
- VOICES nunca abaixa.
- Volta suave (~0,4s) quando a última fala acaba. Abaixa enquanto
  houver qualquer fala tocando.
- Ajustar o quanto abaixa para a fala ficar NÍTIDA sobre effects e
  environment (ex: ~45-55%), ajustável em settings.

## 4. Atenuação das falas — audível num zoom razoável
- Hoje a fala só se ouve bem com o zoom quase colado no ator. Como o
  ouvinte é a câmera (no alto), a curva precisa ser mais GENEROSA para
  as falas serem audíveis num zoom RAZOÁVEL de jogo, não só coladas.
- Ajustar a curva de atenuação das FALAS: aumentar o raio interno
  (onde o volume ainda é cheio) e/ou suavizar a queda, para que numa
  distância normal de jogo (câmera em zoom médio) a fala seja bem
  ouvida.
- Mantém a lógica de atenuar por distância (fala muito longe ainda cai),
  só que generosa o bastante para o zoom de jogo normal.
- Isso vale só para as FALAS (SC_Voice) — não mexer na atenuação dos
  outros sons.

## Entregável
- Menu Options com 4 sliders + 4 checkboxes de mudo (Voices, Music,
  Environment, Effects).
- Defaults: Voices 80, Music 20, Effects 50, Environment 40.
- Ducking: effects e environment abaixam nas falas; música constante;
  voices nunca abaixa.
- Atenuação das falas mais generosa (audível em zoom razoável).
- BD.Test.Regression passa; compilar os dois alvos; commit.
