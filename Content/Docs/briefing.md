# Briefing atual — Brazil Defense

**Versão: 2026-10-12 (depois do "faça")**

> Arquivo sempre sobrescrito. Só o trabalho pendente da vez.

Aplicado e com regressão 102/102 (não commitado): Mito sem estamina chuta
ministro e ignora a horda no retorno; ministros passam por cima dos
separadores; console abre com `'` (ao lado do 1) e F10; Rodadas 8 e 9 na
§13 do PLANO.

# Pendente

## GPU: o tabuleiro ficou mais caro que na Rodada 7
- Rodada 9: sem horda, o mesmo `PerfScene` com 75% custa 22 ms de GPU,
  contra 13,6 ms em 2026-10-11. A horda de ~380 custa só ~2,8 ms.
- Suspeita: as usage flags de Nanite/ISM do `M_Master_Environment`
  (Nicole e urna com o material de verdade no `-game` desde hoje).
- Próximo passo: A/B com `ue_perf.ps1 -Scene horde -Wave 90 -SpawnEvery
  0.45 -ScreenPercentage 0`, escondendo atiradores (`-Hide Shooter`) e a
  urna, para ver qual malha volta os ~8 ms.

## Decisões dele (da Rodada 8)
- Dinheiro sobrando depois da onda 100 (66 mil parados, nenhuma evolução
  comprada): o que fazer com ele.
- 7–8 ministros cobrindo tudo antes do palácio custaram 510 votos nas ondas
  42–43: é o perigo pretendido?
