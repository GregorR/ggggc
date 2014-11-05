/* The Great Computer Language Shootout
   http://shootout.alioth.debian.org/

   contributed by Jarkko Miettinen
*/

public class binarytreestd {

	private final static int minDepth = 4;

	public static void main(String[] args){
		int n = 0;
		if (args.length > 0) n = Integer.parseInt(args[0]);

		int maxDepth = (minDepth + 2 > n) ? minDepth + 2 : n;
		int stretchDepth = maxDepth + 1;

		int check = (TreeNode.topDownTree(0,stretchDepth)).itemCheck();
		System.out.println("stretch tree of depth "+stretchDepth+"\t check: " + check);

		TreeNode longLivedTree = TreeNode.topDownTree(0,maxDepth);

		for (int depth=minDepth; depth<=maxDepth; depth+=2){
			int iterations = 1 << (maxDepth - depth + minDepth);
			check = 0;

			for (int i=1; i<=iterations; i++){
				check += (TreeNode.topDownTree(i,depth)).itemCheck();
				check += (TreeNode.topDownTree(-i,depth)).itemCheck();
			}
			System.out.println((iterations*2) + "\t trees of depth " + depth + "\t check: " + check);
		}
		System.out.println("long lived tree of depth " + maxDepth + "\t check: "+ longLivedTree.itemCheck());
	}


	private static class TreeNode
	{
		public TreeNode left, right;
		private int item;

		TreeNode(int item){
			this.item = item;
		}

		private static TreeNode topDownTree(int item, int depth){
			if (depth>0){
                            TreeNode ret = new TreeNode(item);
                            TreeNode left = topDownTree(2*item-1, depth-1);
                            TreeNode right = topDownTree(2*item, depth-1);
                            ret.left = left;
                            ret.right = right;
                            return ret;
			}
			else {
				return new TreeNode(item);
			}
		}

		TreeNode(TreeNode left, TreeNode right, int item){
			this.left = left;
			this.right = right;
			this.item = item;
		}

		private int itemCheck(){
			// if necessary deallocate here
			if (left==null) return item;
			else return item + left.itemCheck() - right.itemCheck();
		}
	}
}
