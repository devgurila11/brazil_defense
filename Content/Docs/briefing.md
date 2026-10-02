# Briefing atual — Brazil Defense

**Versão: 2026-10-02 18:45**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

# Dois ajustes de fechamento

Teste do conjunto ficou bom. Só estes dois ajustes para fechar a etapa.
(As torres JÁ estão girando — era impressão; não mexer nelas.)

---

## 1. Esc cancela o posicionamento atual

Hoje, com uma peça selecionada para construir (na mão), o clique fica
preso no modo de colocação — mesmo sem recurso (preview vermelho). Isso
impede clicar nas defesas para ver o debug de alcance (o on-click de
seleção).

- ESC cancela o posicionamento atual: a peça na mão some, sai do modo
  de colocação, e o clique volta a selecionar/inspecionar as defesas
  (mostrando o alcance on-click).
- Botão direito também cancela (se já não fizer).
- Assim o jogador nunca fica preso com um preview atrapalhando a
  inspeção das defesas.

---

## 2. Ciclo dia/noite: mais DIA, menos noite

A noite ocupa tempo demais do ciclo (~1/3), então os testes ficam muito
na tela escura. Reduzir a fatia da NOITE no ciclo para ~15-20%, com o
DIA dominante.

- Redistribuir as faixas (Day Cycle > Clock) para passar a maior parte
  das ondas no CLARO; a noite vira um trecho curto e pontual.
- NÃO mexer na intensidade/piso de luz da noite. A noite escura será
  resolvida DEPOIS com os POSTES DE ILUMINAÇÃO (fase futura). Agora é
  só redistribuir o TEMPO, não a luz.

## Entregável
- Esc cancela o posicionamento (clique volta a inspecionar defesas).
- Ciclo com mais dia, noite reduzida (menos tempo na tela escura),
  sem mexer na luz.
- BD.Test.Regression passa; compilar os dois alvos; commit.
