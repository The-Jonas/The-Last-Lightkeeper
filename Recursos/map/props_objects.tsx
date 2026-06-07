<?xml version="1.0" encoding="UTF-8"?>
<tileset version="1.10" tiledversion="1.12.1" name="props_objects" tilewidth="2847" tileheight="1920" tilecount="17" columns="0">
 <grid orientation="orthogonal" width="1" height="1"/>
 <tile id="0" type="Caixa">
  <properties>
   <property name="isStatic" type="bool" value="true"/>
   <property name="z" type="int" value="2"/>
  </properties>
  <image source="../img/objetos/Caixa.png" width="188" height="265"/>
 </tile>
 <tile id="1" type="Pilar">
  <image source="../img/cenario/pilares.png" width="2847" height="1347"/>
 </tile>
 <tile id="2" type="Escada_Quebrada">
  <image source="../img/cenario/escada_quebrada.png" width="2497" height="1553"/>
 </tile>
 <tile id="3">
  <image source="../img/cenario/escada_inteira.png" width="2497" height="1553"/>
 </tile>
 <tile id="4" type="PlayerSpawn_Big">
  <image source="../img/personagens/Irmãozão frente.png" width="127" height="353"/>
 </tile>
 <tile id="5" type="PlayerSpawn_Small">
  <image source="../img/personagens/irmãozinho frente.png" width="126" height="274"/>
 </tile>
 <tile id="6" type="ItemSpawn">
  <properties>
   <property name="depthOffset" type="float" value="0"/>
   <property name="heightLevel" type="int" value="0"/>
   <property name="itemName" value="Broken Flashlight"/>
  </properties>
  <image source="../img/items/Isqueiro.png" width="126" height="141"/>
 </tile>
 <tile id="7" type="ItemSpawn">
  <properties>
   <property name="depthOffset" type="float" value="0"/>
   <property name="heightLevel" type="int" value="0"/>
   <property name="itemName" value="Oil Gallon"/>
  </properties>
  <image source="../img/items/oil_gallon.png" width="1108" height="1920"/>
 </tile>
 <tile id="8">
  <image source="../img/items/apple.png" width="1920" height="1920"/>
 </tile>
 <tile id="9" type="CaixasAmontoadas">
  <image source="../img/objetos/Amontoado_caixas.png" width="443" height="596"/>
 </tile>
 <tile id="10" type="Castical">
  <properties>
   <property name="depthOffset" type="float" value="200"/>
   <property name="direction" value="frente"/>
   <property name="startsLit" type="bool" value="false"/>
  </properties>
  <image source="../img/objetos/castical/castical_frente_apagado.png" width="48" height="95"/>
 </tile>
 <tile id="11" type="Castical">
  <properties>
   <property name="depthOffset" type="float" value="200"/>
   <property name="direction" value="frente"/>
   <property name="startsLit" type="bool" value="true"/>
  </properties>
  <image source="../img/objetos/castical/castical_frente_aceso.png" width="48" height="95"/>
 </tile>
 <tile id="12" type="Castical">
  <properties>
   <property name="direction" value="esquerda"/>
   <property name="startsLit" type="bool" value="false"/>
  </properties>
  <image source="../img/objetos/castical/castical_esquerda_apagado.png" width="55" height="99"/>
 </tile>
 <tile id="13" type="Castical">
  <properties>
   <property name="direction" value="Direita"/>
   <property name="startsLit" type="bool" value="true"/>
  </properties>
  <image source="../img/objetos/castical/castical_esquerda_aceso.png" width="55" height="99"/>
 </tile>
 <tile id="14" type="Castical">
  <properties>
   <property name="direction" value="esquerda"/>
   <property name="startsLit" type="bool" value="false"/>
  </properties>
  <image source="../img/objetos/castical/castical_direita_apagado.png" width="55" height="99"/>
 </tile>
 <tile id="15" type="Castical">
  <properties>
   <property name="direction" value="esquerda"/>
   <property name="startsLit" type="bool" value="true"/>
  </properties>
  <image source="../img/objetos/castical/castical_direita_aceso.png" width="55" height="99"/>
 </tile>
 <tile id="16" type="Armario">
  <image source="../img/objetos/3005_PROP_ARMARIO_FLV.png" width="775" height="746"/>
 </tile>
</tileset>
