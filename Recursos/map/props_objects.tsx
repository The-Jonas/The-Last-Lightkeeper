<?xml version="1.0" encoding="UTF-8"?>
<tileset version="1.10" tiledversion="1.12.1" name="props_objects" tilewidth="2847" tileheight="1920" tilecount="37" columns="0">
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
  <image source="../img/items/lighter_fuel.png" width="38" height="82"/>
 </tile>
 <tile id="8">
  <image source="../img/items/apple.png" width="1920" height="1920"/>
 </tile>
 <tile id="9" type="Recipiente_Decoracao">
  <properties>
   <property name="nameObj" value="Amontoado_caixas"/>
  </properties>
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
  <image source="../img/objetos/armario/armario_frente.png" width="775" height="746"/>
 </tile>
 <tile id="17" type="Mesa">
  <properties>
   <property name="variation" value="normal"/>
  </properties>
  <image source="../img/objetos/mesa/Mesa_normal.png" width="368" height="509"/>
 </tile>
 <tile id="18" type="Cadeira_Caida">
  <image source="../img/objetos/Cadeira_caida.png" width="194" height="302"/>
 </tile>
 <tile id="19" type="Monstro">
  <properties>
   <property name="waypoints" value=""/>
  </properties>
  <image source="../img/personagens/monstro/monstro_placeholder.png" width="574" height="561"/>
 </tile>
 <tile id="20">
  <image source="../img/cenario/escada_tabua.png" width="2497" height="1553"/>
 </tile>
 <tile id="21" type="Barril">
  <properties>
   <property name="type" type="int" value="0"/>
  </properties>
  <image source="../img/objetos/barril/barril_0.png" width="235" height="343"/>
 </tile>
 <tile id="22" type="Barril">
  <properties>
   <property name="type" type="int" value="1"/>
  </properties>
  <image source="../img/objetos/barril/barril_1.png" width="608" height="630"/>
 </tile>
 <tile id="23" type="Barril">
  <properties>
   <property name="type" type="int" value="2"/>
  </properties>
  <image source="../img/objetos/barril/barril_2.png" width="472" height="642"/>
 </tile>
 <tile id="24" type="Barril">
  <properties>
   <property name="type" type="int" value="3"/>
  </properties>
  <image source="../img/objetos/barril/barril_3.png" width="536" height="548"/>
 </tile>
 <tile id="25" type="Mesa">
  <properties>
   <property name="variation" value="mapa"/>
  </properties>
  <image source="../img/objetos/mesa/Mesa_mapa.png" width="252" height="446"/>
 </tile>
 <tile id="26" type="Pesca_Asset">
  <properties>
   <property name="nameObject" value="baldes"/>
  </properties>
  <image source="../img/objetos/pesca/baldes.png" width="430" height="419"/>
 </tile>
 <tile id="27" type="Pesca_Asset">
  <properties>
   <property name="nameObject" value="boia"/>
  </properties>
  <image source="../img/objetos/pesca/boia.png" width="269" height="284"/>
 </tile>
 <tile id="28" type="Pesca_Asset">
  <properties>
   <property name="nameObject" value="boias"/>
  </properties>
  <image source="../img/objetos/pesca/boias.png" width="431" height="284"/>
 </tile>
 <tile id="29" type="Pesca_Asset">
  <properties>
   <property name="nameObject" value="cesta_corda"/>
  </properties>
  <image source="../img/objetos/pesca/cesta_corda.png" width="314" height="255"/>
 </tile>
 <tile id="30" type="Pesca_Asset">
  <properties>
   <property name="nameObject" value="cestas_corda"/>
  </properties>
  <image source="../img/objetos/pesca/cestas_corda.png" width="438" height="428"/>
 </tile>
 <tile id="31" type="Pesca_Asset">
  <properties>
   <property name="nameObject" value="rede_parede"/>
  </properties>
  <image source="../img/objetos/pesca/rede_parede.png" width="850" height="987"/>
 </tile>
 <tile id="32" type="Pesca_Asset">
  <properties>
   <property name="nameObject" value="rede_pendurada"/>
  </properties>
  <image source="../img/objetos/pesca/rede_pendurada.png" width="193" height="502"/>
 </tile>
 <tile id="33" type="Pesca_Asset">
  <properties>
   <property name="nameObject" value="redes"/>
  </properties>
  <image source="../img/objetos/pesca/redes.png" width="302" height="338"/>
 </tile>
 <tile id="34" type="Pesca_Asset">
  <properties>
   <property name="nameObject" value="vara_pesca"/>
  </properties>
  <image source="../img/objetos/pesca/vara_pesca.png" width="134" height="538"/>
 </tile>
 <tile id="35" type="Recipiente_Decoracao">
  <properties>
   <property name="nameObj" value="Bau"/>
  </properties>
  <image source="../img/objetos/Bau.png" width="498" height="500"/>
 </tile>
 <tile id="37" type="Recipiente_Decoracao">
  <properties>
   <property name="nameObj" value="Estante_Quebrada"/>
  </properties>
  <image source="../img/objetos/Estante_quebrada.png" width="686" height="555"/>
 </tile>
</tileset>
