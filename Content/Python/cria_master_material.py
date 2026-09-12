"""
cria_master_material.py  -  Brazil Defense

Cria M_Master_Environment em /Game/BD/Materials com todos os canais
parametrizados e texturas SEPARADAS (sem ORM empacotado).

COMO RODAR:
    No editor:
    1. Editar > Plugins > habilite "Python Editor Script Plugin" e reinicie.
    2. Tools > Execute Python Script, escolha este arquivo
       (ou cole o caminho no Output Log, aba Cmd em modo Python).

    Sem o editor aberto (commandlet, ~30 s):
    UnrealEditor-Cmd.exe Brazil_Defense.uproject -run=pythonscript
        -script="<caminho deste arquivo>" -unattended -nullrhi

Se voce ja tem texturas importadas, aponte os caminhos em TEXTURAS abaixo.
Deixando vazio, ele usa placeholders e o material compila igual.

ORDEM NOS SAMPLERS: a textura default e definida ANTES do sampler type.
Um nó com textura sRGB/RGBA e sampler Linear Grayscale nao compila, e um
save nesse estado trava o editor. Por isso os canais de dados usam
T_White_Grayscale (4x4 G8, sRGB off) e nunca WhiteSquareTexture.
"""

import unreal

# ---------------------------------------------------------------- config

PASTA = "/Game/BD/Materials"
NOME = "M_Master_Environment"

# True = reaproveita e reconstroi o grafo se ja existir (util para rodar de
# novo apos ajuste). As material instances continuam apontando para ele.
SOBRESCREVER = True

# Coloque aqui os caminhos dos seus assets, ex: "/Game/BD/Textures/T_Calcada_D"
# Vazio = usa placeholder.
TEXTURAS = {
    "base_color": "",
    "normal": "",
    "roughness": "",
    "metallic": "",
    "ao": "",
}

# Canais de DADOS (roughness, metallic, AO) precisam de placeholder
# grayscale linear. WhiteSquareTexture e sRGB/RGBA: com sampler type
# Linear Grayscale o material NAO compila e cai no Default Material.
FALLBACK = {
    "base_color": "/Engine/EngineResources/WhiteSquareTexture",
    "normal": "/Engine/EngineMaterials/FlatNormal",
    "roughness": "/Game/BD/Textures/T_White_Grayscale",
    "metallic": "/Game/BD/Textures/T_White_Grayscale",
    "ao": "/Game/BD/Textures/T_White_Grayscale",
}

FLATTEN_NORMAL = ("/Engine/Functions/Engine_MaterialFunctions01/Texturing/"
                  "FlattenNormal.FlattenNormal")

MEL = unreal.MaterialEditingLibrary
falhas = []

# ---------------------------------------------------------------- helpers


def carregar_textura(slot):
    caminho = TEXTURAS.get(slot) or FALLBACK[slot]
    tex = unreal.EditorAssetLibrary.load_asset(caminho)
    if tex is None:
        unreal.log_warning("Textura nao encontrada: %s" % caminho)
    return tex


def tipo_sampler(tex, normal=False):
    """Escolhe o sampler type pelo flag sRGB da propria textura.

    Assim funciona tanto com o placeholder (sRGB) quanto com as suas
    texturas reais de roughness/AO (lineares), sem erro de compilacao.
    """
    if normal:
        return unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    try:
        srgb = tex.get_editor_property("srgb")
    except Exception:
        srgb = True
    if srgb:
        return unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    # textura de canal unico (G8 / compressao Grayscale) pede
    # LINEAR_GRAYSCALE; LINEAR_COLOR nao casa e quebra a compilacao
    try:
        comp = tex.get_editor_property("compression_settings")
        if comp == unreal.TextureCompressionSettings.TC_GRAYSCALE:
            return unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE
    except Exception:
        pass
    return unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR


def no(mat, classe, x, y, **props):
    exp = MEL.create_material_expression(mat, classe, x, y)
    for k, v in props.items():
        exp.set_editor_property(k, v)
    return exp


def param(mat, classe, x, y, nome, grupo, ordem, **props):
    exp = no(mat, classe, x, y, parameter_name=nome, group=grupo,
             sort_priority=ordem, **props)
    return exp


def modo_sampler_wrap():
    """Samplers compartilhados nao gastam os 16 slots do shader.

    O nome do enum ja mudou entre versoes, entao tenta as variantes
    conhecidas antes de desistir e usar o padrao da textura.
    """
    for nome in ("SSM_WRAP_WORLD_GROUP_SETTINGS", "SSM_WRAP",
                 "SSM_FROM_TEXTURE_ASSET"):
        valor = getattr(unreal.SamplerSourceMode, nome, None)
        if valor is not None:
            return valor
    return None


def sampler(mat, x, y, nome, grupo, ordem, slot, normal=False):
    tex = carregar_textura(slot)
    exp = param(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y,
                nome, grupo, ordem)
    if tex:
        # A textura vem PRIMEIRO: o sampler type e escolhido por ela, e um
        # sampler grayscale sobre uma textura de cor nao compila.
        exp.set_editor_property("texture", tex)
        exp.set_editor_property("sampler_type", tipo_sampler(tex, normal))
    modo = modo_sampler_wrap()
    if modo is not None:
        try:
            exp.set_editor_property("sampler_source", modo)
        except Exception as e:
            unreal.log_warning("sampler_source em %s: %s" % (nome, e))
    return exp


def liga(origem, pino_saida, destino, pino_entrada):
    ok = MEL.connect_material_expressions(origem, pino_saida,
                                          destino, pino_entrada)
    if not ok:
        falhas.append("%s[%s] -> %s[%s]" % (
            origem.get_class().get_name(), pino_saida,
            destino.get_class().get_name(), pino_entrada))
    return ok


def liga_saida(origem, pino_saida, mat, prop):
    ok = MEL.connect_material_property(origem, pino_saida, prop)
    if not ok:
        falhas.append("%s[%s] -> %s" % (
            origem.get_class().get_name(), pino_saida, str(prop)))
    return ok


# ---------------------------------------------------------------- material

caminho_asset = "%s/%s" % (PASTA, NOME)

if unreal.EditorAssetLibrary.does_asset_exist(caminho_asset):
    if not SOBRESCREVER:
        raise Exception("%s ja existe. Ligue SOBRESCREVER para reaproveitar."
                        % caminho_asset)
    # NAO apagar o asset: deletar pacote em uso trava no ForceDeleteObjects.
    # Reaproveita o material e limpa so o grafo interno.
    mat = unreal.EditorAssetLibrary.load_asset(caminho_asset)
    # delete_all_material_expressions deixa nos para tras (parametros
    # duplicados no grafo novo); apagar um a um e conferir e o que funciona.
    for _exp in list(MEL.get_material_expressions(mat)):
        MEL.delete_material_expression(mat, _exp)
    _restantes = len(MEL.get_material_expressions(mat))
    if _restantes:
        raise Exception("%d no(s) do grafo antigo nao foram apagados; "
                        "abortando para nao duplicar parametros." % _restantes)
    unreal.log("Grafo anterior limpo em %s" % caminho_asset)
else:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NOME, PASTA, unreal.Material, unreal.MaterialFactoryNew())

P = unreal.MaterialProperty

# ---- bloco de UV --------------------------------------------------------

tc = no(mat, unreal.MaterialExpressionTextureCoordinate, -1500, 0)

tiling_u = param(mat, unreal.MaterialExpressionScalarParameter, -1500, 150,
                 "TilingU", "01 - UV", 0, default_value=1.0)
tiling_v = param(mat, unreal.MaterialExpressionScalarParameter, -1500, 250,
                 "TilingV", "01 - UV", 1, default_value=1.0)
append_t = no(mat, unreal.MaterialExpressionAppendVector, -1250, 200)
liga(tiling_u, "", append_t, "A")
liga(tiling_v, "", append_t, "B")

mul_uv = no(mat, unreal.MaterialExpressionMultiply, -1050, 100)
liga(tc, "", mul_uv, "A")
liga(append_t, "", mul_uv, "B")

offset_u = param(mat, unreal.MaterialExpressionScalarParameter, -1500, 350,
                 "OffsetU", "01 - UV", 2, default_value=0.0)
offset_v = param(mat, unreal.MaterialExpressionScalarParameter, -1500, 450,
                 "OffsetV", "01 - UV", 3, default_value=0.0)
append_o = no(mat, unreal.MaterialExpressionAppendVector, -1250, 400)
liga(offset_u, "", append_o, "A")
liga(offset_v, "", append_o, "B")

uvs = no(mat, unreal.MaterialExpressionAdd, -850, 200)
liga(mul_uv, "", uvs, "A")
liga(append_o, "", uvs, "B")

# ---- base color ---------------------------------------------------------

tex_bc = sampler(mat, -600, -700, "BaseColorTexture", "02 - Base color", 0,
                 "base_color")
liga(uvs, "", tex_bc, "UVs")

tint = param(mat, unreal.MaterialExpressionVectorParameter, -600, -520,
             "BaseColorTint", "02 - Base color", 1,
             default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
mul_tint = no(mat, unreal.MaterialExpressionMultiply, -350, -650)
liga(tex_bc, "RGB", mul_tint, "A")
liga(tint, "", mul_tint, "B")

brilho = param(mat, unreal.MaterialExpressionScalarParameter, -600, -420,
               "Brightness", "02 - Base color", 2, default_value=1.0)
mul_brilho = no(mat, unreal.MaterialExpressionMultiply, -150, -650)
liga(mul_tint, "", mul_brilho, "A")
liga(brilho, "", mul_brilho, "B")

bc_const = param(mat, unreal.MaterialExpressionVectorParameter, -350, -450,
                 "BaseColorConstant", "02 - Base color", 3,
                 default_value=unreal.LinearColor(0.5, 0.5, 0.5, 1.0))

sw_bc = param(mat, unreal.MaterialExpressionStaticSwitchParameter, 100, -650,
              "UseBaseColorTexture", "00 - Switches", 0, default_value=True)
liga(mul_brilho, "", sw_bc, "True")
liga(bc_const, "", sw_bc, "False")
liga_saida(sw_bc, "", mat, P.MP_BASE_COLOR)

# ---- normal -------------------------------------------------------------

tex_n = sampler(mat, -600, -300, "NormalTexture", "03 - Normal", 0,
                "normal", normal=True)
liga(uvs, "", tex_n, "UVs")

normal_int = param(mat, unreal.MaterialExpressionScalarParameter, -600, -120,
                   "NormalIntensity", "03 - Normal", 1, default_value=1.0)
# FlattenNormal usa Flatness invertido: 0 = normal cheia, 1 = plana
inv = no(mat, unreal.MaterialExpressionOneMinus, -400, -120)
liga(normal_int, "", inv, "")

fn = unreal.EditorAssetLibrary.load_asset(FLATTEN_NORMAL)
flatten = no(mat, unreal.MaterialExpressionMaterialFunctionCall, -120, -280,
             material_function=fn)
liga(tex_n, "RGB", flatten, "Normal")
liga(inv, "", flatten, "Flatness")

# normal plana em tangent space, para quando nao ha textura
n_const = no(mat, unreal.MaterialExpressionConstant3Vector, -144, -112,
             constant=unreal.LinearColor(0.0, 0.0, 1.0, 1.0))

sw_n = param(mat, unreal.MaterialExpressionStaticSwitchParameter, 180, -280,
             "UseNormalTexture", "00 - Switches", 1, default_value=True)
liga(flatten, "", sw_n, "True")
liga(n_const, "", sw_n, "False")
liga_saida(sw_n, "", mat, P.MP_NORMAL)

# ---- roughness ----------------------------------------------------------

tex_r = sampler(mat, -600, 60, "RoughnessTexture", "04 - Roughness", 0,
                "roughness")
liga(uvs, "", tex_r, "UVs")

# a textura vira alpha de um lerp entre min e max: remapeia sem curva
r_min = param(mat, unreal.MaterialExpressionScalarParameter, -600, 350,
              "RoughMin", "04 - Roughness", 1, default_value=0.0)
r_max = param(mat, unreal.MaterialExpressionScalarParameter, -600, 450,
              "RoughMax", "04 - Roughness", 2, default_value=1.0)
lerp_r = no(mat, unreal.MaterialExpressionLinearInterpolate, -250, 150)
liga(r_min, "", lerp_r, "A")
liga(r_max, "", lerp_r, "B")
liga(tex_r, "R", lerp_r, "Alpha")

r_const = param(mat, unreal.MaterialExpressionScalarParameter, -250, 320,
                "RoughnessConstant", "04 - Roughness", 3, default_value=0.5)

sw_r = param(mat, unreal.MaterialExpressionStaticSwitchParameter, 100, 150,
             "UseRoughnessTexture", "00 - Switches", 2, default_value=True)
liga(lerp_r, "", sw_r, "True")
liga(r_const, "", sw_r, "False")
liga_saida(sw_r, "", mat, P.MP_ROUGHNESS)

# ---- metallic -----------------------------------------------------------

tex_m = sampler(mat, -600, 560, "MetallicTexture", "05 - Metallic", 0,
                "metallic")
liga(uvs, "", tex_m, "UVs")

m_const = param(mat, unreal.MaterialExpressionScalarParameter, -250, 700,
                "MetallicConstant", "05 - Metallic", 1, default_value=0.0)

# desligado por padrao: a maior parte do cenario e dieletrica
sw_m = param(mat, unreal.MaterialExpressionStaticSwitchParameter, 100, 580,
             "UseMetallicTexture", "00 - Switches", 3, default_value=False)
liga(tex_m, "R", sw_m, "True")
liga(m_const, "", sw_m, "False")
liga_saida(sw_m, "", mat, P.MP_METALLIC)

# ---- ambient occlusion --------------------------------------------------

tex_ao = sampler(mat, -600, 850, "AOTexture", "06 - Ambient occlusion", 0,
                 "ao")
liga(uvs, "", tex_ao, "UVs")

# AO = lerp(1, textura, intensidade): 0 desliga, 1 usa a textura inteira
ao_int = param(mat, unreal.MaterialExpressionScalarParameter, -600, 1100,
               "AOIntensity", "06 - Ambient occlusion", 1, default_value=1.0)
um = no(mat, unreal.MaterialExpressionConstant, -600, 1200, r=1.0)
lerp_ao = no(mat, unreal.MaterialExpressionLinearInterpolate, -250, 950)
liga(um, "", lerp_ao, "A")
liga(tex_ao, "R", lerp_ao, "B")
liga(ao_int, "", lerp_ao, "Alpha")

sw_ao = param(mat, unreal.MaterialExpressionStaticSwitchParameter, 100, 930,
              "UseAOTexture", "00 - Switches", 4, default_value=True)
liga(lerp_ao, "", sw_ao, "True")
liga(um, "", sw_ao, "False")
liga_saida(sw_ao, "", mat, P.MP_AMBIENT_OCCLUSION)

# ---------------------------------------------------------------- fim

MEL.recompile_material(mat)
salvo = unreal.EditorAssetLibrary.save_asset(caminho_asset,
                                             only_if_is_dirty=False)

if falhas:
    unreal.log_error("Ligacoes que falharam (%d):" % len(falhas))
    for f in falhas:
        unreal.log_error("  " + f)
else:
    unreal.log("%s: grafo criado, todas as ligacoes ok, salvo=%s"
               % (caminho_asset, salvo))

# nao deixa referencia a nos do material pendurada no console do editor
for _v in ("tc", "tiling_u", "tiling_v", "append_t", "mul_uv", "offset_u",
           "offset_v", "append_o", "uvs", "tex_bc", "tint",
           "mul_tint", "brilho", "mul_brilho", "bc_const", "sw_bc",
           "tex_n", "normal_int", "inv", "fn", "flatten", "n_const", "sw_n",
           "tex_r", "r_min", "r_max", "lerp_r", "r_const", "sw_r",
           "tex_m", "m_const", "sw_m",
           "tex_ao", "ao_int", "um", "lerp_ao", "sw_ao"):
    globals().pop(_v, None)
