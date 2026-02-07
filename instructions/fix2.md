
same as fix1, make sure the code is patched
build-linux/_deps/grpc-src/third_party/abseil-cpp/absl/strings/internal/str_format/extension.h
local copy:

explanation of the patch

add #include <cstdint> on top

more

==== linux target
grpc-src/third_party/abseil-cpp/absl/strings/internal/str_format/extension.cc:44:13: error: ‘FormatConversionCharSet’ does not name a type; did you mean ‘FormatConversionCharInternal’? 44 | constexpr FormatConversionCharSet FormatConversionCharSetInternal::c; | ^~~~~~~~~~~~~~~~~~~~~~~
Cette erreur survient généralement lors de la compilation d'une ancienne version de gRPC (ou de sa dépendance Abseil) avec un compilateur récent comme GCC 13+. 
Le problème provient d'un changement dans la gestion des dépendances d'en-têtes et de la syntaxe des énumérations scopées dans les versions récentes de GCC, ce qui empêche le compilateur de reconnaître le type FormatConversionCharSet. 
Solutions recommandées
Mise à jour (Recommandé) :
La solution la plus propre est de mettre à jour gRPC vers une version récente (ex: v1.69.0+) ou de forcer la mise à jour du sous-module Abseil. Ce bug a été corrigé dans Abseil via le commit b957f0c.
Correction manuelle (Patch) :
Si vous devez absolument conserver cette version, vous pouvez appliquer un patch sur le fichier absl/strings/internal/str_format/extension.h :
Ajout d'en-tête : Ajoutez #include <cstdint> au début du fichier.
Correction de syntaxe : Remplacez les déclarations d'énumérations scopées mal formées. Par exemple, si vous voyez enum class FormatConversionChar : uint8_t;, assurez-vous que la définition complète est visible ou retirez le mot-clé class dans les déclarations anticipées si nécessaire selon votre version de standard C++.
Utilisation du standard C++11 :
Dans certains cas, forcer le compilateur à utiliser C++11 via l'indicateur --std=c++11 dans vos CMAKE_CXX_FLAGS peut contourner les restrictions strictes de GCC 13. 
Souhaitez-vous les instructions spécifiques pour appliquer ce patch via un fichier .patch ou préférez-vous de l'aide pour mettre à jour vos sous-modules Git ? 




