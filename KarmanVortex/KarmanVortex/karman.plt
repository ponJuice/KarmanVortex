# karman.plt

#-------------------------------------------------------------------------------
# ループ処理
#-------------------------------------------------------------------------------
if(exist("n")==0 || n<0) n = n0 # ループ変数の初期化

#-------------------------------------------------------------------------------
# プロット
#-------------------------------------------------------------------------------
filename = sprintf("v_%d.dat", n) # n番目のデータファイルの名前の生成
plot filename every 4 using 1:2:(adj*$3):(adj*$4) with vec # n番目のデータのプロット
#-------------------------------------------------------------------------------
# ループ処理
#-------------------------------------------------------------------------------
n = n + dn            # ループ変数の増加
if ( n < n1 ) reread  # ループの評価
undefine n            # ループ変数の削除
